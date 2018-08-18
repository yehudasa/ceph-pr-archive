// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */
#ifndef CEPH_MONCLIENT_H
#define CEPH_MONCLIENT_H

#include <memory>

#include "msg/Messenger.h"

#include "MonMap.h"
#include "MonSub.h"

#include "common/async/completion.h"
#include "common/Timer.h"
#include "common/config.h"
#include "messages/MMonGetVersion.h"

class MMonMap;
class MConfig;
class MMonGetVersionReply;
struct MMonSubscribeAck;
class MMonCommandAck;
struct MAuthReply;
class MAuthRotating;
class LogClient;
class AuthAuthorizer;
class AuthMethodList;
class AuthClientHandler;
class KeyRing;
class RotatingKeyRing;

struct MonClientPinger : public Dispatcher {
  Mutex lock;
  Cond ping_recvd_cond;
  string *result;
  bool done;

  MonClientPinger(CephContext *cct_, string *res_) :
    Dispatcher(cct_),
    lock("MonClientPinger::lock"),
    result(res_),
    done(false)
  { }

  int wait_for_reply(double timeout = 0.0) {
    utime_t until = ceph_clock_now();
    until += (timeout > 0 ? timeout : cct->_conf->client_mount_timeout);
    done = false;

    int ret = 0;
    while (!done) {
      ret = ping_recvd_cond.WaitUntil(lock, until);
      if (ret == ETIMEDOUT)
        break;
    }
    return ret;
  }

  bool ms_dispatch(Message *m) override {
    std::lock_guard l(lock);
    if (m->get_type() != CEPH_MSG_PING)
      return false;

    bufferlist &payload = m->get_payload();
    if (result && payload.length() > 0) {
      auto p = std::cbegin(payload);
      decode(*result, p);
    }
    done = true;
    ping_recvd_cond.SignalAll();
    m->put();
    return true;
  }
  bool ms_handle_reset(Connection *con) override {
    std::lock_guard l(lock);
    done = true;
    ping_recvd_cond.SignalAll();
    return true;
  }
  void ms_handle_remote_reset(Connection *con) override {}
  bool ms_handle_refused(Connection *con) override {
    return false;
  }
};

class MonConnection {
public:
  MonConnection(CephContext *cct,
		ConnectionRef conn,
		uint64_t global_id);
  ~MonConnection();
  MonConnection(MonConnection&& rhs) = default;
  MonConnection& operator=(MonConnection&&) = default;
  MonConnection(const MonConnection& rhs) = delete;
  MonConnection& operator=(const MonConnection&) = delete;
  int handle_auth(MAuthReply *m,
		  const EntityName& entity_name,
		  uint32_t want_keys,
		  RotatingKeyRing* keyring);
  int authenticate(MAuthReply *m);
  void start(epoch_t epoch,
             const EntityName& entity_name,
             const AuthMethodList& auth_supported);
  bool have_session() const;
  uint64_t get_global_id() const {
    return global_id;
  }
  ConnectionRef get_con() {
    return con;
  }
  std::unique_ptr<AuthClientHandler>& get_auth() {
    return auth;
  }

private:
  int _negotiate(MAuthReply *m,
		 const EntityName& entity_name,
		 uint32_t want_keys,
		 RotatingKeyRing* keyring);

private:
  CephContext *cct;
  enum class State {
    NONE,
    NEGOTIATING,
    AUTHENTICATING,
    HAVE_SESSION,
  };
  State state = State::NONE;
  ConnectionRef con;

  std::unique_ptr<AuthClientHandler> auth;
  uint64_t global_id;
};

class MonClient : public Dispatcher {
public:
  // Error, Newest, Oldest
  using VersionSig = void(boost::system::error_code, version_t, version_t);
  using VersionCompletion = ceph::async::Completion<VersionSig>;

  using CommandSig = void(boost::system::error_code, std::string, ceph::bufferlist);
  using CommandCompletion = ceph::async::Completion<CommandSig>;

  MonMap monmap;
  map<string,string> config_mgr;
private:
  Messenger *messenger;

  std::unique_ptr<MonConnection> active_con; std::map<entity_addr_t, MonConnection> pending_cons;

  EntityName entity_name;

  entity_addr_t my_addr;

  mutable Mutex monc_lock;
  SafeTimer timer;
  boost::asio::io_context& service;

  bool initialized;
  bool no_keyring_disabled_cephx;

  LogClient *log_client;
  bool more_log_pending;

  void send_log(bool flush = false);

  std::unique_ptr<AuthMethodList> auth_supported;

  bool ms_dispatch(Message *m) override;
  bool ms_handle_reset(Connection *con) override;
  void ms_handle_remote_reset(Connection *con) override {}
  bool ms_handle_refused(Connection *con) override { return false; }

  void handle_monmap(MMonMap *m);
  void handle_config(MConfig *m);

  void handle_auth(MAuthReply *m);

  // monitor session
  void tick();
  void schedule_tick();

  // monclient
  bool want_monmap;
  Cond map_cond;
  bool passthrough_monmap = false;
  bool got_config = false;

  // authenticate
  std::unique_ptr<AuthClientHandler> auth;
  uint32_t want_keys = 0;
  uint64_t global_id = 0;
  Cond auth_cond;
  int authenticate_err = 0;
  bool authenticated = false;

  list<Message*> waiting_for_session;
  utime_t last_rotating_renew_sent;
  std::unique_ptr<Context> session_established_context;
  bool had_a_connection;
  double reopen_interval_multiplier;

  bool _opened() const;
  bool _hunting() const;
  void _start_hunting();
  void _finish_hunting();
  void _finish_auth(int auth_err);
  void _reopen_session(int rank = -1);
  MonConnection& _add_conn(unsigned rank, uint64_t global_id);
  void _un_backoff();
  void _add_conns(uint64_t global_id);
  void _send_mon_message(Message *m);

  std::map<entity_addr_t, MonConnection>::iterator _find_pending_con(
    const ConnectionRef& con) {
    for (auto i = pending_cons.begin(); i != pending_cons.end(); ++i) {
      if (i->second.get_con() == con) {
	return i;
      }
    }
    return pending_cons.end();
  }

public:
  void set_entity_name(EntityName name) { entity_name = name; }

  int _check_auth_tickets();
  int _check_auth_rotating();
  int wait_auth_rotating(double timeout);

  int authenticate(double timeout=0.0);
  bool is_authenticated() const {return authenticated;}

  bool is_connected() const { return active_con != nullptr; }

  /**
   * Try to flush as many log messages as we can in a single
   * message.  Use this before shutting down to transmit your
   * last message.
   */
  void flush_log();

private:
  // mon subscriptions
  MonSub sub;
  void _renew_subs();
  void handle_subscribe_ack(MMonSubscribeAck* m);

public:
  void renew_subs() {
    std::lock_guard l(monc_lock);
    _renew_subs();
  }
  bool sub_want(string what, version_t start, unsigned flags) {
    std::lock_guard l(monc_lock);
    return sub.want(what, start, flags);
  }
  void sub_got(string what, version_t have) {
    std::lock_guard l(monc_lock);
    sub.got(what, have);
  }
  void sub_unwant(string what) {
    std::lock_guard l(monc_lock);
    sub.unwant(what);
  }
  bool sub_want_increment(string what, version_t start, unsigned flags) {
    std::lock_guard l(monc_lock);
    return sub.inc_want(what, start, flags);
  }
  
  std::unique_ptr<KeyRing> keyring;
  std::unique_ptr<RotatingKeyRing> rotating_secrets;

 public:
  MonClient(CephContext *cct_, boost::asio::io_context& service);
  MonClient(const MonClient &) = delete;
  MonClient& operator=(const MonClient &) = delete;
  ~MonClient() override;

  int init();
  void shutdown();

  void set_log_client(LogClient *clog) {
    log_client = clog;
  }

  int build_initial_monmap();
  int get_monmap();
  int get_monmap_and_config();
  /**
   * If you want to see MonMap messages, set this and
   * the MonClient will tell the Messenger it hasn't
   * dealt with it.
   * Note that if you do this, *you* are of course responsible for
   * putting the message reference!
   */
  void set_passthrough_monmap() {
    std::lock_guard l(monc_lock);
    passthrough_monmap = true;
  }
  void unset_passthrough_monmap() {
    std::lock_guard l(monc_lock);
    passthrough_monmap = false;
  }
  /**
   * Ping monitor with ID @p mon_id and record the resulting
   * reply in @p result_reply.
   *
   * @param[in]  mon_id Target monitor's ID
   * @param[out] result_reply reply from mon.ID, if param != NULL
   * @returns    0 in case of success; < 0 in case of error,
   *             -ETIMEDOUT if monitor didn't reply before timeout
   *             expired (default: conf->client_mount_timeout).
   */
  int ping_monitor(const string &mon_id, string *result_reply);

  void send_mon_message(Message *m) {
    std::lock_guard l(monc_lock);
    _send_mon_message(m);
  }
  /**
   * If you specify a callback, you should not call
   * reopen_session() again until it has been triggered. The MonClient
   * will behave, but the first callback could be triggered after
   * the session has been killed and the MonClient has started trying
   * to reconnect to another monitor.
   */
  void reopen_session(Context *cb=NULL) {
    std::lock_guard l(monc_lock);
    if (cb) {
      session_established_context.reset(cb);
    }
    _reopen_session();
  }

  entity_addr_t get_my_addr() const {
    return my_addr;
  }

  const uuid_d& get_fsid() const {
    return monmap.fsid;
  }

  entity_addrvec_t get_mon_addrs(unsigned i) const {
    std::lock_guard l(monc_lock);
    if (i < monmap.size())
      return monmap.get_addrs(i);
    return entity_addrvec_t();
  }
  int get_num_mon() const {
    std::lock_guard l(monc_lock);
    return monmap.size();
  }

  uint64_t get_global_id() const {
    std::lock_guard l(monc_lock);
    return global_id;
  }

  void set_messenger(Messenger *m) { messenger = m; }
  entity_addr_t get_myaddr() const { return messenger->get_myaddr(); }
  entity_addrvec_t get_myaddrs() const { return messenger->get_myaddrs(); }
  AuthAuthorizer* build_authorizer(int service_id) const;

  void set_want_keys(uint32_t want) {
    want_keys = want;
  }

  // admin commands
private:
  uint64_t last_mon_command_tid;

  struct MonCommand {
    string target_name;
    int target_rank = -1;
    uint64_t tid;
    vector<string> cmd;
    bufferlist inbl;
    std::unique_ptr<CommandCompletion> onfinish;
    std::optional<boost::asio::steady_timer> cancel_timer;

    MonCommand(MonClient& monc, uint64_t t,
	       std::unique_ptr<CommandCompletion> onfinish)
      : tid(t), onfinish(std::move(onfinish)) {
      auto timeout = ceph::maybe_timespan(monc.cct->_conf->rados_mon_op_timeout);
      if (timeout) {
	cancel_timer.emplace(monc.service, *timeout);
	cancel_timer->async_wait(
          [this, &monc](boost::system::error_code ec) {
	    if (ec)
	      return;
	    Mutex::Locker l(monc.monc_lock);
	    monc._cancel_mon_command(tid);
	  });
      }
    }
  };
  friend MonCommand;
  map<uint64_t,MonCommand*> mon_commands;

  void _send_command(MonCommand *r);
  void _resend_mon_commands();
  int _cancel_mon_command(uint64_t tid);
  void _finish_command(MonCommand *r, boost::system::error_code ret, string&& rs, bufferlist&& bl);
  void _finish_auth();
  void handle_mon_command_ack(MMonCommandAck *ack);

public:
  template<typename CompletionToken>
  auto start_mon_command(const vector<string>& cmd, const bufferlist& inbl,
			 CompletionToken&& token) {
    auto l = std::scoped_lock(monc_lock);
    boost::asio::async_completion<CompletionToken, CommandSig> init(token);
    auto r = new MonCommand(*this, ++last_mon_command_tid,
			    CommandCompletion::create(service.get_executor(),
						      std::move(init.completion_handler)));
    r->cmd = cmd;
    r->inbl = inbl;
    mon_commands.emplace(r->tid, r);
    _send_command(r);
    return init.result.get();
  }

  template<typename CompletionToken>
  auto start_mon_command(int mon_rank, const vector<string>& cmd,
			 const bufferlist& inbl, CompletionToken&& token) {
    auto l = std::scoped_lock(monc_lock);
    boost::asio::async_completion<CompletionToken, CommandSig> init(token);
    auto r = new MonCommand(*this, ++last_mon_command_tid,
			    CommandCompletion::create(service.get_executor(),
						      std::move(init.completion_handler)));
    r->target_rank = mon_rank;
    r->cmd = cmd;
    r->inbl = inbl;
    mon_commands.emplace(r->tid, r);
    _send_command(r);
    return init.result.get();
  }

  template<typename CompletionToken>
  auto start_mon_command(const string &mon_name, const vector<string>& cmd,
			 const bufferlist& inbl, CompletionToken&& token) {
    auto l = std::scoped_lock(monc_lock);
    boost::asio::async_completion<CompletionToken, CommandSig> init(token);
    auto r = new MonCommand(*this, ++last_mon_command_tid,
			    CommandCompletion::create(service.get_executor(),
						      std::move(init.completion_handler)));
    r->target_name = mon_name;
    r->cmd = cmd;
    r->inbl = inbl;
    mon_commands.emplace(r->tid, r);
    _send_command(r);
    return init.result.get();
  }

  class ContextVerter {
    std::string* outs;
    ceph::bufferlist* outbl;
    Context* onfinish;

  public:
    ContextVerter(std::string* outs, ceph::bufferlist* outbl, Context* onfinish)
      : outs(outs), outbl(outbl), onfinish(onfinish) {}
    ~ContextVerter() = default;
    ContextVerter(const ContextVerter&) = default;
    ContextVerter& operator =(const ContextVerter&) = default;
    ContextVerter(ContextVerter&&) = default;
    ContextVerter& operator =(ContextVerter&&) = default;

    void operator()(boost::system::error_code e,
		    std::string&& s,
		    ceph::bufferlist&& bl) {
      if (outs)
	*outs = std::move(s);
      if (outbl)
	*outbl = std::move(bl);
      if (onfinish)
	onfinish->complete(ceph::from_error_code(e));
    }
  };

  void start_mon_command(const vector<string>& cmd, const bufferlist& inbl,
			 bufferlist *outbl, string *outs,
			 Context *onfinish) {
    start_mon_command(cmd, inbl, ContextVerter(outs, outbl, onfinish));
  }
  void start_mon_command(int mon_rank,
			 const vector<string>& cmd, const bufferlist& inbl,
			 bufferlist *outbl, string *outs,
			 Context *onfinish) {
    start_mon_command(mon_rank, cmd, inbl, ContextVerter(outs, outbl, onfinish));
  }
  void start_mon_command(const string &mon_name,  ///< mon name, with mon. prefix
			 const vector<string>& cmd, const bufferlist& inbl,
			 bufferlist *outbl, string *outs,
			 Context *onfinish) {
    start_mon_command(mon_name, cmd, inbl, ContextVerter(outs, outbl, onfinish));
  }


  // version requests
public:
  /**
   * get latest known version(s) of cluster map
   *
   * @param map string name of map (e.g., 'osdmap')
   * @param token context that will be triggered on completion
   * @return (via Completion) {} on success,
   *         boost::system::errc::resource_unavailable_try_again if we need to
   *         resubmit our request
   */
  template<typename CompletionToken>
  auto get_version(string&& map, CompletionToken&& token) {
    std::lock_guard l(monc_lock);
    MMonGetVersion *m = new MMonGetVersion();
    m->what = std::move(map);
    m->handle = ++version_req_id;
    boost::asio::async_completion<CompletionToken, VersionSig> init(token);
    version_requests.emplace(m->handle,
			     VersionCompletion::create(service.get_executor(),
						       std::move(init.completion_handler)));
    _send_mon_message(m);
    return init.result.get();
  }

  /**
   * Run a callback within our lock, with a reference
   * to the MonMap
   */
  template<typename Callback, typename...Args>
  auto with_monmap(Callback&& cb, Args&&...args) const ->
    decltype(cb(monmap, std::forward<Args>(args)...)) {
    std::lock_guard l(monc_lock);
    return std::forward<Callback>(cb)(monmap, std::forward<Args>(args)...);
  }

  void register_config_callback(md_config_t::config_callback fn);
  md_config_t::config_callback get_config_callback();

private:

  std::map<ceph_tid_t, std::unique_ptr<VersionCompletion>> version_requests;
  ceph_tid_t version_req_id;
  void handle_get_version_reply(MMonGetVersionReply* m);
  md_config_t::config_callback config_cb;
};

const boost::system::error_category& monc_category() noexcept;

namespace monc_errc {
enum monc_errc_t {
  shutting_down = 1, // Command failed due to MonClient shutting down
  session_reset, // Monitor session was reset
  rank_dne, // Requested monitor rank does not exist
  mon_dne, // Requested monitor does not exist
  timed_out // Monitor operation timed out
};
}

namespace boost {
namespace system {
template<>
struct is_error_code_enum<::monc_errc::monc_errc_t> {
  static const bool value = true;
};
}
}

namespace monc_errc {
//  explicit conversion:
inline boost::system::error_code make_error_code(monc_errc_t e) noexcept {
  return { e, monc_category() };
}

// implicit conversion:
inline boost::system::error_condition make_error_condition(monc_errc_t e)
  noexcept {
  return { e, monc_category() };
}
}

const boost::system::error_category& monc_category() noexcept;

#endif
