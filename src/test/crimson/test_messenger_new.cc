#include "messages/MPing.h"
#include "crimson/net/Connection.h"
#include "crimson/net/Dispatcher.h"
#include "crimson/net/SocketMessenger.h"

#include <map>
#include <random>
#include <boost/program_options.hpp>
#include <seastar/core/app-template.hh>
#include <seastar/core/do_with.hh>
#include <seastar/core/future-util.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/sleep.hh>

namespace bpo = boost::program_options;

namespace {

template <typename T, typename... Args>
seastar::future<T*> create_sharded(Args... args) {
  auto sharded_obj = seastar::make_lw_shared<seastar::sharded<T>>();
  return sharded_obj->start(args...).then([sharded_obj]() {
      auto& ret = sharded_obj->local();
      seastar::engine().at_exit([sharded_obj]() {
          return sharded_obj->stop().finally([sharded_obj] {});
        });
      return &ret;
    });
}

std::random_device rd;
std::default_random_engine rng{rd()};
bool verbose = false;

seastar::future<> test_echo(unsigned rounds,
                            double keepalive_ratio)
{
  struct test_state {
    struct ServerDispatcher final
        : public ceph::net::Dispatcher,
          public seastar::peering_sharded_service<ServerDispatcher> {
      seastar::future<> ms_dispatch(ceph::net::ConnectionRef c,
                                    MessageRef m) override {
        if (verbose) {
          std::cout << "server got " << *m << std::endl;
        }
        // reply with a pong
        return c->send(MessageRef{new MPing(), false});
      }
      Dispatcher* get_local_shard() override {
        return &(container().local());
      }
      seastar::future<> stop() {
        return seastar::make_ready_future<>();
      }
    };

    struct PingClient {
      ceph::net::Connection* conn;
      unsigned rounds;
      std::bernoulli_distribution keepalive_dist;
      unsigned count = 0u;
      seastar::promise<MessageRef> reply;

      PingClient(ceph::net::Connection* conn, unsigned rounds, double keepalive_ratio)
        : conn(conn),
          rounds(rounds),
          keepalive_dist(std::bernoulli_distribution{keepalive_ratio}) {}

      seastar::future<> pingpong() {
        return seastar::repeat([this] {
          if (keepalive_dist(rng)) {
            return conn->keepalive().then([] {
              return seastar::make_ready_future<seastar::stop_iteration>(
                seastar::stop_iteration::no);
              });
          } else {
            return conn->send(MessageRef{new MPing(), false}).then([&] {
              return reply.get_future();
            }).then([&] (MessageRef msg) {
              reply = seastar::promise<MessageRef>{};
              if (verbose) {
                std::cout << "client got reply " << *msg << std::endl;
              }
              return seastar::make_ready_future<seastar::stop_iteration>(
                  seastar::stop_iteration::yes);
            });
          };
        });
      }
      bool done() const {
        return count >= rounds;
      }
    };

    struct ClientDispatcher final
        : public ceph::net::Dispatcher,
          public seastar::peering_sharded_service<ClientDispatcher> {
      seastar::promise<MessageRef> reply;
      unsigned count = 0u;
      std::map<ceph::net::Connection*, PingClient*> clients;

      seastar::future<> ms_dispatch(ceph::net::ConnectionRef c,
                                    MessageRef m) override {
        auto found = clients.find(&*c);
        ceph_assert(found != clients.end());
        PingClient *client = found->second;
        ++(client->count);
        if (verbose) {
          std::cout << "client ms_dispatch " << client->count << std::endl;
        }
        client->reply.set_value(std::move(m));
        return seastar::now();
      }
      Dispatcher* get_local_shard() override {
        return &(container().local());
      }
      seastar::future<> stop() {
        return seastar::make_ready_future<>();
      }

      void register_client(PingClient *client) {
        clients.emplace(client->conn, client);
      }
      void unregister_client(PingClient *client) {
        auto found = clients.find(client->conn);
        ceph_assert(found != clients.end());
        clients.erase(found);
      }
    };

    static seastar::future<> dispatch_pingpong(ceph::net::ConnectionXRef conn,
                                               ClientDispatcher *dispatcher,
                                               unsigned rounds,
                                               double keepalive_ratio) {
        return seastar::smp::submit_to(conn->get()->shard_id(),
                                       [conn=&**conn, dispatcher, rounds, keepalive_ratio] {
            ClientDispatcher *local_disp = &(dispatcher->container().local());
            return seastar::do_with(PingClient(conn, rounds, keepalive_ratio),
                                    [local_disp](auto &client) {
                local_disp->register_client(&client);
                return seastar::repeat([&client] {
                    return client.pingpong()
                      .then([&client] {
                        return seastar::make_ready_future<seastar::stop_iteration>(
                          client.done() ?
                          seastar::stop_iteration::yes :
                          seastar::stop_iteration::no);
                      });
                  }).finally([local_disp, &client] {
                    local_disp->unregister_client(&client);
                  });
              });
          }).finally([conn] {});
    }
  };

  typedef std::tuple<seastar::future<ceph::net::SocketMessenger*>,
                     seastar::future<test_state::ServerDispatcher*>,
                     seastar::future<ceph::net::SocketMessenger*>,
                     seastar::future<test_state::ServerDispatcher*>,
                     seastar::future<ceph::net::SocketMessenger*>,
                     seastar::future<test_state::ClientDispatcher*>,
                     seastar::future<ceph::net::SocketMessenger*>,
                     seastar::future<test_state::ClientDispatcher*>> result_tuple;
  typedef std::tuple<seastar::future<ceph::net::ConnectionXRef>,
                     seastar::future<ceph::net::ConnectionXRef>,
                     seastar::future<ceph::net::ConnectionXRef>,
                     seastar::future<ceph::net::ConnectionXRef>> result_conns;
  return seastar::when_all(
      create_sharded<ceph::net::SocketMessenger>(entity_name_t::OSD(0)),
      create_sharded<test_state::ServerDispatcher>(),
      create_sharded<ceph::net::SocketMessenger>(entity_name_t::OSD(1)),
      create_sharded<test_state::ServerDispatcher>(),
      create_sharded<ceph::net::SocketMessenger>(entity_name_t::OSD(10)),
      create_sharded<test_state::ClientDispatcher>(),
      create_sharded<ceph::net::SocketMessenger>(entity_name_t::OSD(11)),
      create_sharded<test_state::ClientDispatcher>())
    .then([rounds, keepalive_ratio](result_tuple t) {
      auto server_msgr1 = std::get<0>(t).get0();
      auto server_disp1 = std::get<1>(t).get0();
      auto server_msgr2 = std::get<2>(t).get0();
      auto server_disp2 = std::get<3>(t).get0();
      auto client_msgr1 = std::get<4>(t).get0();
      auto client_disp1 = std::get<5>(t).get0();
      auto client_msgr2 = std::get<6>(t).get0();
      auto client_disp2 = std::get<7>(t).get0();
      // start servers
      entity_addr_t addr;
      addr.set_family(AF_INET);
      addr.set_port(9010);
      return server_msgr1->bind(addr)
        .then([server_msgr1, server_disp1] {
          return server_msgr1->start(server_disp1);
        }).then([server_msgr2] {
          entity_addr_t addr;
          addr.set_family(AF_INET);
          addr.set_port(9011);
          return server_msgr2->bind(addr);
        }).then([server_msgr2, server_disp2] {
          return server_msgr2->start(server_disp2);
      // start clients
        }).then([client_msgr1, client_disp1] {
          return client_msgr1->start(client_disp1);
        }).then([client_msgr2, client_disp2] {
          return client_msgr2->start(client_disp2);
      // connect clients to servers
        }).then([client_msgr1, client_msgr2] {
          entity_addr_t peer_addr1;
          peer_addr1.parse("127.0.0.1:9010", nullptr);
          entity_addr_t peer_addr2;
          peer_addr2.parse("127.0.0.1:9011", nullptr);
          return seastar::when_all(
              client_msgr1->connect(peer_addr1, entity_name_t::TYPE_OSD),
              client_msgr1->connect(peer_addr2, entity_name_t::TYPE_OSD),
              client_msgr2->connect(peer_addr1, entity_name_t::TYPE_OSD),
              client_msgr2->connect(peer_addr2, entity_name_t::TYPE_OSD));
      // send pingpong from clients to servers
        }).then([client_disp1, client_disp2, rounds, keepalive_ratio](result_conns conns) {
          auto msgr1_conn1 = std::get<0>(conns).get0();
          auto msgr1_conn2 = std::get<1>(conns).get0();
          auto msgr2_conn1 = std::get<2>(conns).get0();
          auto msgr2_conn2 = std::get<3>(conns).get0();
          std::cout << "clients connected, start pingpong..." << std::endl;
          return seastar::when_all_succeed(
              test_state::dispatch_pingpong(msgr1_conn1, client_disp1, rounds, keepalive_ratio),
              test_state::dispatch_pingpong(msgr1_conn2, client_disp1, rounds, keepalive_ratio),
              test_state::dispatch_pingpong(msgr2_conn1, client_disp2, rounds, keepalive_ratio),
              test_state::dispatch_pingpong(msgr2_conn2, client_disp2, rounds, keepalive_ratio));
      // shutdown
        }).then([client_msgr1] {
          std::cout << "client_msgr1 shutdown..." << std::endl;
          return client_msgr1->shutdown();
        }).then([client_msgr2] {
          std::cout << "client_msgr2 shutdown..." << std::endl;
          return client_msgr2->shutdown();
        }).then([server_msgr1] {
          std::cout << "server_msgr1 shutdown..." << std::endl;
          return server_msgr1->shutdown();
        }).then([server_msgr2] {
          std::cout << "server_msgr2 shutdown..." << std::endl;
          return server_msgr2->shutdown();
        });
    });
}

}

int main(int argc, char** argv)
{
  seastar::app_template app;
  app.add_options()
    ("verbose,v", bpo::value<bool>()->default_value(false),
     "chatty if true")
    ("rounds", bpo::value<unsigned>()->default_value(512),
     "number of pingpong rounds")
    ("keepalive-ratio", bpo::value<double>()->default_value(0.1),
     "ratio of keepalive in ping messages");
  return app.run(argc, argv, [&app] {
      auto&& config = app.configuration();
      verbose = config["verbose"].as<bool>();
      auto rounds = config["rounds"].as<unsigned>();
      auto keepalive_ratio = config["keepalive-ratio"].as<double>();
      return test_echo(rounds, keepalive_ratio)
        .then([] {
        //  return test_concurrent_dispatch();
        //}).then([] {
          std::cout << "All tests succeeded" << std::endl;
        }).handle_exception([] (auto eptr) {
          std::cout << "Test failure" << std::endl;
          return seastar::make_exception_future<>(eptr);
        });
    });
}
