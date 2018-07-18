// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-

#pragma once

#include <type_traits>
#include "common/config.h"
#include "common/config_fwd.h"
#include "common/config_obs.h"
#include "common/config_obs_mgr.h"
#include "common/Mutex.h"
#include "common/CondVar.h"

// @c ConfigProxy is a facade of multiple config related classes. it exposes
// the legacy settings with arrow operator, and the new-style config with its
// member methods.
class ConfigProxy {
  /**
   * The current values of all settings described by the schema
   */
  ConfigValues values;
  ObserverMgr<md_config_obs_t> obs_mgr;
  md_config_t config;
  /** A lock that protects the md_config_t internals. It is
   * recursive, for simplicity.
   * It is best if this lock comes first in the lock hierarchy.
   * This lock is dropped before calling config observers. */
  mutable Mutex lock;

  class ObsCallTracker {
  private:
    uint32_t call_count = 0;
    Mutex lock;
    Cond cond;
  public:
    ObsCallTracker()
      : lock("obs_call_tracker", false, true) {
    }

    void start() {
      Mutex::Locker locker(lock);
      ++call_count;
    }
    void finish() {
      Mutex::Locker locker(lock);
      assert(call_count > 0);
      if (--call_count == 0) {
        cond.Signal();
      }
    }
    void wait() {
      lock.Lock();
      while (call_count != 0) {
        cond.Wait(lock);
      }
      lock.Unlock();
    }
  };

  void observer_call_start(md_config_obs_t *obs) {
    auto p = obs_call_track.find(obs);
    assert(p != obs_call_track.end());
    p->second->start();
  }
  void observer_call_finish(md_config_obs_t *obs) {
    auto p = obs_call_track.find(obs);
    assert(p != obs_call_track.end());
    p->second->finish();
  }
  void wait_for_observer_calls(md_config_obs_t *obs) {
    auto p = obs_call_track.find(obs);
    assert(p != obs_call_track.end());
    p->second->wait();
  }

  typedef std::unique_ptr<ObsCallTracker> ObsCallTrackerRef;

  std::map<md_config_obs_t *, ObsCallTrackerRef> obs_call_track;

  void call_observers(ObserverMgr<md_config_obs_t>::rev_obs_map_t &rev_obs) {
    assert(!lock.is_locked());

    for (auto& [obs, keys] : rev_obs) {
      obs->handle_conf_change(*this, keys);
      // this can be done outside the lock as observer_call_start()
      // and remove_observer() are serialized via lock
      observer_call_finish(obs);
    }
  }

  void map_observer_changes(md_config_obs_t *obs, const std::string &key,
                            ObserverMgr<md_config_obs_t>::rev_obs_map_t *rev_obs) {
    assert(lock.is_locked());

    auto [it, new_entry] = rev_obs->emplace(obs, std::set<std::string>{});
    (*it).second.emplace(key);
    if (new_entry) {
      // this needs to be done under lock as once this lock is
      // dropped (before calling observers) a remove_observer()
      // can sneak in and cause havoc.
      observer_call_start(obs);
    }
  }

public:
  explicit ConfigProxy(bool is_daemon)
    : config{values, obs_mgr, is_daemon},
      lock{"ConfigProxy", true, false}
  {}
  const ConfigValues* operator->() const noexcept {
    return &values;
  }
  ConfigValues* operator->() noexcept {
    return &values;
  }
  int get_val(const std::string& key, char** buf, int len) const {
    Mutex::Locker l{lock};
    return config.get_val(values, key, buf, len);
  }
  int get_val(const std::string &key, std::string *val) const {
    Mutex::Locker l{lock};
    return config.get_val(values, key, val);
  }
  template<typename T>
  const T get_val(const std::string& key) const {
    Mutex::Locker l{lock};
    return config.template get_val<T>(values, key);
  }
  template<typename T, typename Callback, typename...Args>
  auto with_val(const string& key, Callback&& cb, Args&&... args) const {
    Mutex::Locker l{lock};
    return config.template with_val<T>(values, key,
				       std::forward<Callback>(cb),
				       std::forward<Args>(args)...);
  }
  void config_options(Formatter *f) const {
    config.config_options(f);
  }
  const Option* get_schema(const std::string& key) const {
    auto found = config.schema.find(key);
    if (found == config.schema.end()) {
      return nullptr;
    } else {
      return &found->second;
    }
  }
  const Option *find_option(const string& name) const {
    return config.find_option(name);
  }
  void diff(Formatter *f, const std::string& name=string{}) const {
    Mutex::Locker l{lock};
    return config.diff(values, f, name);
  }
  void get_my_sections(std::vector <std::string> &sections) const {
    Mutex::Locker l{lock};
    config.get_my_sections(values, sections);
  }
  int get_all_sections(std::vector<std::string>& sections) const {
    Mutex::Locker l{lock};
    return config.get_all_sections(sections);
  }
  int get_val_from_conf_file(const std::vector<std::string>& sections,
			     const std::string& key, std::string& out,
			     bool emeta) const {
    Mutex::Locker l{lock};
    return config.get_val_from_conf_file(values,
					 sections, key, out, emeta);
  }
  unsigned get_osd_pool_default_min_size() const {
    return config.get_osd_pool_default_min_size(values);
  }
  void early_expand_meta(std::string &val,
			 std::ostream *oss) const {
    Mutex::Locker l{lock};
    return config.early_expand_meta(values, val, oss);
  }
  // for those want to reexpand special meta, e.g, $pid
  void finalize_reexpand_meta() {
    ObserverMgr<md_config_obs_t>::rev_obs_map_t rev_obs;
    {
      Mutex::Locker l(lock);
      if (config.finalize_reexpand_meta(values, obs_mgr)) {
        _gather_changes(values.changed, &rev_obs, nullptr);
        values.changed.clear();
      }
    }

    call_observers(rev_obs);
  }
  void add_observer(md_config_obs_t* obs) {
    Mutex::Locker l(lock);
    obs_mgr.add_observer(obs);
    obs_call_track.emplace(obs, std::make_unique<ObsCallTracker>());
  }
  void remove_observer(md_config_obs_t* obs) {
    Mutex::Locker l(lock);
    wait_for_observer_calls(obs);
    obs_call_track.erase(obs);
    obs_mgr.remove_observer(obs);
  }
  void call_all_observers() {
    ObserverMgr<md_config_obs_t>::rev_obs_map_t rev_obs;
    {
      Mutex::Locker l(lock);
      obs_mgr.gather_all_observer_changes(
        [this, &rev_obs](md_config_obs_t *obs, const std::string &key) {
          map_observer_changes(obs, key, &rev_obs);
        });
    }

    call_observers(rev_obs);
  }
  void set_safe_to_start_threads() {
    config.set_safe_to_start_threads();
  }
  void _clear_safe_to_start_threads() {
    config._clear_safe_to_start_threads();
  }
  void show_config(std::ostream& out) {
    Mutex::Locker l{lock};
    config.show_config(values, out);
  }
  void show_config(Formatter *f) {
    Mutex::Locker l{lock};
    config.show_config(values, f);
  }
  void config_options(Formatter *f) {
    Mutex::Locker l{lock};
    config.config_options(f);
  }
  int rm_val(const std::string& key) {
    Mutex::Locker l{lock};
    return config.rm_val(values, key);
  }
  // Expand all metavariables. Make any pending observer callbacks.
  void apply_changes(std::ostream* oss) {
    ObserverMgr<md_config_obs_t>::rev_obs_map_t rev_obs;
    {
      Mutex::Locker l{lock};
      // apply changes until the cluster name is assigned
      if (!values.cluster.empty()) {
        // meta expands could have modified anything.  Copy it all out again.
        _gather_changes(values.changed, &rev_obs, oss);
        values.changed.clear();
      }
    }

    call_observers(rev_obs);
  }
  void _gather_changes(std::set<std::string> &changes,
                       ObserverMgr<md_config_obs_t>::rev_obs_map_t *rev_obs,
                       std::ostream* oss) {
    obs_mgr.gather_changes(
      changes, *this,
      [this, rev_obs](md_config_obs_t *obs, const std::string &key) {
        map_observer_changes(obs, key, rev_obs);
      }, oss);
  }
  int set_val(const std::string& key, const std::string& s,
              std::stringstream* err_ss=nullptr) {
    Mutex::Locker l{lock};
    return config.set_val(values, obs_mgr, key, s);
  }
  void set_val_default(const std::string& key, const std::string& val) {
    Mutex::Locker l{lock};
    config.set_val_default(values, obs_mgr, key, val);
  }
  void set_val_or_die(const std::string& key, const std::string& val) {
    Mutex::Locker l{lock};
    config.set_val_or_die(values, obs_mgr, key, val);
  }
  int set_mon_vals(CephContext *cct,
		   const map<std::string,std::string>& kv,
		   md_config_t::config_callback config_cb) {
    int ret;
    ObserverMgr<md_config_obs_t>::rev_obs_map_t rev_obs;
    {
      Mutex::Locker l{lock};
      ret = config.set_mon_vals(cct, values, obs_mgr, kv, config_cb);
      _gather_changes(values.changed, &rev_obs, nullptr);
      values.changed.clear();
    }

    call_observers(rev_obs);
    return ret;
  }
  int injectargs(const std::string &s, std::ostream *oss) {
    int ret;
    ObserverMgr<md_config_obs_t>::rev_obs_map_t rev_obs;
    {
      Mutex::Locker l{lock};
      ret = config.injectargs(values, obs_mgr, s, oss);
      _gather_changes(values.changed, &rev_obs, oss);
      values.changed.clear();
    }

    call_observers(rev_obs);
    return ret;
  }
  void parse_env(const char *env_var = "CEPH_ARGS") {
    Mutex::Locker l{lock};
    config.parse_env(values, obs_mgr, env_var);
  }
  int parse_argv(std::vector<const char*>& args, int level=CONF_CMDLINE) {
    Mutex::Locker l{lock};
    return config.parse_argv(values, obs_mgr, args, level);
  }
  int parse_config_files(const char *conf_files,
			 std::ostream *warnings, int flags) {
    Mutex::Locker l{lock};
    return config.parse_config_files(values, obs_mgr,
				     conf_files, warnings, flags);
  }
  size_t num_parse_errors() const {
    return config.parse_errors.size();
  }
  void complain_about_parse_errors(CephContext *cct) {
    return config.complain_about_parse_errors(cct);
  }
  void do_argv_commands() const {
    Mutex::Locker l{lock};
    config.do_argv_commands(values);
  }
  void get_config_bl(uint64_t have_version,
		     bufferlist *bl,
		     uint64_t *got_version) {
    Mutex::Locker l{lock};
    config.get_config_bl(values, have_version, bl, got_version);
  }
  void get_defaults_bl(bufferlist *bl) {
    Mutex::Locker l{lock};
    config.get_defaults_bl(values, bl);
  }
};
