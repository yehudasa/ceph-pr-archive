// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-

#pragma once

#include <seastar/core/reactor.hh>
#include <seastar/core/sharded.hh>
#include "common/config.h"
#include "common/config_obs.h"
#include "common/config_obs_mgr.h"
#include "common/errno.h"

namespace ceph::common {

// a facade for managing config. each shard has its own copy of ConfigProxy.
//
// In seastar-osd, there could be multiple instances of @c ConfigValues in a
// single process, as we are using a variant of read-copy-update mechinary to
// update the settings at runtime.
class ConfigProxy : public seastar::peering_sharded_service<ConfigProxy>
{
  using LocalConfigValues = seastar::lw_shared_ptr<ConfigValues>;
  seastar::foreign_ptr<LocalConfigValues> values;

  md_config_t* remote_config = nullptr;
  std::unique_ptr<md_config_t> local_config;

  using ConfigObserver = ceph::md_config_obs_impl<ConfigProxy>;
  ObserverMgr<ConfigObserver> obs_mgr;

  const md_config_t& get_config() const {
    return remote_config ? *remote_config : * local_config;
  }
  md_config_t& get_config() {
    return remote_config ? *remote_config : * local_config;
  }

  // apply changes to all shards
  // @param func a functor which accepts @c "ConfigValues&"
  template<typename Func>
  seastar::future<> do_change(Func&& func) {
    return container().invoke_on(values.get_owner_shard(),
                                 [func = std::move(func)](ConfigProxy& owner) {
      // apply the changes to a copy of the values
      auto new_values = seastar::make_lw_shared(*owner.values);
      new_values->changed.clear();
      func(*new_values);

      ObserverMgr<ConfigObserver>::rev_obs_map_t rev_obs;

      // always apply the new settings synchronously on the owner shard, to
      // avoid racings with other do_change() calls in parallel.
      owner.values.reset(new_values);
      owner.obs_mgr.gather_changes(owner.values->changed, owner,
                                   [&rev_obs](ConfigObserver *obs,
                                              const std::string &key) {
                                     rev_obs[obs].insert(key);
                                   }, nullptr);
      for (auto& [obs, keys] : rev_obs) {
        obs->handle_conf_change(owner, keys);
      }

      rev_obs.clear();

      return seastar::parallel_for_each(boost::irange(1u, seastar::smp::count),
                                        [&owner, new_values, &rev_obs] (auto cpu) {
        return owner.container().invoke_on(cpu,
          [foreign_values = seastar::make_foreign(new_values), &rev_obs](ConfigProxy& proxy) mutable {
            proxy.values.reset();
            proxy.values = std::move(foreign_values);
            proxy.obs_mgr.gather_changes(proxy.values->changed, proxy,
                                         [&rev_obs](ConfigObserver *obs,
                                                    const std::string &key) {
                                           rev_obs[obs].insert(key);
                                         }, nullptr);
          });
        }).finally([new_values, &owner, &rev_obs] {
          for (auto& [obs, keys] : rev_obs) {
            obs->handle_conf_change(owner, keys);
          }
          new_values->changed.clear();
        });
      });
  }
public:
  ConfigProxy();
  const ConfigValues* operator->() const noexcept {
    return values.get();
  }
  ConfigValues* operator->() noexcept {
    return values.get();
  }

  // required by sharded<>
  seastar::future<> start();
  seastar::future<> stop() {
    return seastar::make_ready_future<>();
  }
  void add_observer(ConfigObserver* obs) {
    obs_mgr.add_observer(obs);
  }
  void remove_observer(ConfigObserver* obs) {
    obs_mgr.remove_observer(obs);
  }
  seastar::future<> rm_val(const std::string& key) {
    return do_change([key, this](ConfigValues& values) {
      auto ret = get_config().rm_val(values, key);
      if (ret < 0) {
        throw std::invalid_argument(cpp_strerror(ret));
      }
    });
  }
  seastar::future<> set_val(const std::string& key,
			    const std::string& val) {
    return do_change([key, val, this](ConfigValues& values) {
      std::stringstream err;
      auto ret = get_config().set_val(values, obs_mgr, key, val, &err);
      if (ret < 0) {
	throw std::invalid_argument(err.str());
      }
    });
  }
  int get_val(const std::string &key, std::string *val) const {
    return get_config().get_val(*values, key, val);
  }
  template<typename T>
  const T get_val(const std::string& key) const {
    return get_config().template get_val<T>(*values, key);
  }

  seastar::future<> set_mon_vals(const std::map<std::string,std::string>& kv) {
    return do_change([kv, this](ConfigValues& values) {
      get_config().set_mon_vals(nullptr, values, obs_mgr, kv, nullptr);
    });
  }

  using ShardedConfig = seastar::sharded<ConfigProxy>;

private:
  static ShardedConfig sharded_conf;
  friend ConfigProxy& local_conf();
  friend ShardedConfig& sharded_conf();
};

inline ConfigProxy& local_conf() {
  return ConfigProxy::sharded_conf.local();
}

inline ConfigProxy::ShardedConfig& sharded_conf() {
  return ConfigProxy::sharded_conf;
}

}
