#include "rgw_sync_module_pubsub.h"
#include "rgw_sync_module_pubsub_rest.h"
#include "rgw_pubsub.h"
#include "rgw_op.h"
#include "rgw_rest.h"
#include "rgw_rest_s3.h"

#define dout_context g_ceph_context
#define dout_subsys ceph_subsys_rgw


class RGWPSCreateTopicOp : public RGWOp {
protected:
  std::unique_ptr<RGWUserPubSub> ups;
  string topic_name;
  string bucket_name;
  RGWBucketInfo bucket_info;

public:
  RGWPSCreateTopicOp() {}

  int verify_permission() override {
    int ret = get_params();
    if (ret < 0) {
      return ret;
    }

    RGWObjectCtx& obj_ctx = *static_cast<RGWObjectCtx *>(s->obj_ctx);

    ret = store->get_bucket_info(obj_ctx, s->owner.get_id().tenant, bucket_name,
                                 bucket_info, nullptr, nullptr);
    if (ret < 0) {
      return ret;
    }

    if (bucket_info.owner != s->owner.get_id()) {
      ldout(s->cct, 20) << "user doesn't own bucket, cannot create topic" << dendl;
      return -EPERM;
    }

    return 0;
  }
  void pre_exec() override {
    rgw_bucket_object_pre_exec(s);
  }
  void execute() override;

  const char* name() const override { return "pubsub_topic_create"; }
  virtual RGWOpType get_type() override { return RGW_OP_PUBSUB_TOPIC_CREATE; }
  virtual uint32_t op_mask() override { return RGW_OP_TYPE_WRITE; }
  virtual int get_params() = 0;
};

void RGWPSCreateTopicOp::execute()
{
  ups = make_unique<RGWUserPubSub>(store, s->owner.get_id());
  op_ret = ups->create_topic(topic_name, bucket_info.bucket);
  if (op_ret < 0) {
    ldout(s->cct, 20) << "failed to create topic, ret=" << op_ret << dendl;
    return;
  }
}

class RGWPSCreateTopic_ObjStore_S3 : public RGWPSCreateTopicOp {
public:
  explicit RGWPSCreateTopic_ObjStore_S3() {}

  int get_params() override {
    topic_name = s->object.name;

    bool exists;
    bucket_name = s->info.args.get("bucket", &exists);
    if (!exists) {
      ldout(s->cct, 20) << "ERROR: missing required param 'bucket' for request" << dendl;
      return -EINVAL;
    }

    return 0;
  }
};

class RGWPSListTopicsOp : public RGWOp {
protected:
  string bucket_name;
  RGWBucketInfo bucket_info;
  std::unique_ptr<RGWUserPubSub> ups;
  rgw_pubsub_user_topics result;


public:
  RGWPSListTopicsOp() {}

  int verify_permission() override {
    int ret = get_params();
    if (ret < 0) {
      return ret;
    }

    if (bucket_name.empty()) {
      return 0;
    }

    RGWObjectCtx& obj_ctx = *static_cast<RGWObjectCtx *>(s->obj_ctx);

    ret = store->get_bucket_info(obj_ctx, s->owner.get_id().tenant, bucket_name,
                                 bucket_info, nullptr, nullptr);
    if (ret < 0) {
      return ret;
    }

    if (bucket_info.owner != s->owner.get_id()) {
      ldout(s->cct, 20) << "user doesn't own bucket, cannot create topic" << dendl;
      return -EPERM;
    }

    return 0;
  }
  void pre_exec() override {
    rgw_bucket_object_pre_exec(s);
  }
  void execute() override;

  const char* name() const override { return "pubsub_topics_list"; }
  virtual RGWOpType get_type() override { return RGW_OP_PUBSUB_TOPICS_LIST; }
  virtual uint32_t op_mask() override { return RGW_OP_TYPE_READ; }
  virtual int get_params() = 0;
};

void RGWPSListTopicsOp::execute()
{
  ups = make_unique<RGWUserPubSub>(store, s->owner.get_id());
  if (bucket_name.empty()) {
    op_ret = ups->get_topics(&result);
  } else {
    op_ret = ups->get_bucket_topics(bucket_info.bucket, &result);
  }
  if (op_ret < 0) {
    ldout(s->cct, 20) << "failed to get topics, ret=" << op_ret << dendl;
    return;
  }

}

class RGWPSListTopics_ObjStore_S3 : public RGWPSListTopicsOp {
public:
  explicit RGWPSListTopics_ObjStore_S3() {}

  int get_params() override {
    bucket_name = s->info.args.get("bucket");
    return 0;
  }

  void send_response() override {
    if (op_ret) {
      set_req_state_err(s, op_ret);
    }
    dump_errno(s);
    end_header(s, this, "application/json");

    if (op_ret < 0) {
      return;
    }

    encode_json("result", result, s->formatter);
    rgw_flush_formatter_and_reset(s, s->formatter);
  }
};

class RGWPSGetTopicOp : public RGWOp {
protected:
  string topic_name;
  std::unique_ptr<RGWUserPubSub> ups;
  rgw_pubsub_user_topic_info result;

public:
  RGWPSGetTopicOp() {}

  int verify_permission() override {
    return 0;
  }
  void pre_exec() override {
    rgw_bucket_object_pre_exec(s);
  }
  void execute() override;

  const char* name() const override { return "pubsub_topic_get"; }
  virtual RGWOpType get_type() override { return RGW_OP_PUBSUB_TOPIC_GET; }
  virtual uint32_t op_mask() override { return RGW_OP_TYPE_READ; }
  virtual int get_params() = 0;
};

void RGWPSGetTopicOp::execute()
{
  op_ret = get_params();
  if (op_ret < 0) {
    return;
  }
  ups = make_unique<RGWUserPubSub>(store, s->owner.get_id());
  op_ret = ups->get_topic(topic_name, &result);
  if (op_ret < 0) {
    ldout(s->cct, 20) << "failed to get topic, ret=" << op_ret << dendl;
    return;
  }
}

class RGWPSGetTopic_ObjStore_S3 : public RGWPSGetTopicOp {
public:
  explicit RGWPSGetTopic_ObjStore_S3() {}

  int get_params() override {
    topic_name = s->object.name;
    return 0;
  }

  void send_response() override {
    if (op_ret) {
      set_req_state_err(s, op_ret);
    }
    dump_errno(s);
    end_header(s, this, "application/json");

    if (op_ret < 0) {
      return;
    }

    encode_json("result", result, s->formatter);
    rgw_flush_formatter_and_reset(s, s->formatter);
  }
};

class RGWPSDeleteTopicOp : public RGWOp {
protected:
  string topic_name;
  std::unique_ptr<RGWUserPubSub> ups;

public:
  RGWPSDeleteTopicOp() {}

  int verify_permission() override {
    return 0;
  }
  void pre_exec() override {
    rgw_bucket_object_pre_exec(s);
  }
  void execute() override;

  const char* name() const override { return "pubsub_topic_delete"; }
  virtual RGWOpType get_type() override { return RGW_OP_PUBSUB_TOPIC_DELETE; }
  virtual uint32_t op_mask() override { return RGW_OP_TYPE_DELETE; }
  virtual int get_params() = 0;
};

void RGWPSDeleteTopicOp::execute()
{
  op_ret = get_params();
  if (op_ret < 0) {
    return;
  }

  ups = make_unique<RGWUserPubSub>(store, s->owner.get_id());
  op_ret = ups->remove_topic(topic_name);
  if (op_ret < 0) {
    ldout(s->cct, 20) << "failed to create topic, ret=" << op_ret << dendl;
    return;
  }
}

class RGWPSDeleteTopic_ObjStore_S3 : public RGWPSDeleteTopicOp {
public:
  explicit RGWPSDeleteTopic_ObjStore_S3() {}

  int get_params() override {
    topic_name = s->object.name;
    return 0;
  }
};

class RGWHandler_REST_PSTopic_S3 : public RGWHandler_REST_S3 {
protected:
  bool supports_quota() override {
    return false;
  }
  RGWOp *op_get() override {
    if (s->init_state.url_bucket.empty()) {
      return nullptr;
    }
    if (s->object.empty()) {
      return new RGWPSListTopics_ObjStore_S3();
    }
    return new RGWPSGetTopic_ObjStore_S3();
  }
  RGWOp *op_put() override {
    if (!s->object.empty()) {
      return new RGWPSCreateTopic_ObjStore_S3();
    }
    return nullptr;
  }
  RGWOp *op_delete() override {
    if (!s->object.empty()) {
      return new RGWPSDeleteTopic_ObjStore_S3();
    }
    return nullptr;
  }
public:
  explicit RGWHandler_REST_PSTopic_S3(const rgw::auth::StrategyRegistry& auth_registry) : RGWHandler_REST_S3(auth_registry) {}
  virtual ~RGWHandler_REST_PSTopic_S3() {}
};


class RGWPSCreateSubOp : public RGWOp {
protected:
  string sub_name;
  string topic_name;
  std::unique_ptr<RGWUserPubSub> ups;
  rgw_pubsub_user_sub_dest dest;

public:
  RGWPSCreateSubOp() {}

  int verify_permission() override {
    return 0;
  }
  void pre_exec() override {
    rgw_bucket_object_pre_exec(s);
  }
  void execute() override;

  const char* name() const override { return "pubsub_subscription_create"; }
  virtual RGWOpType get_type() override { return RGW_OP_PUBSUB_SUB_CREATE; }
  virtual uint32_t op_mask() override { return RGW_OP_TYPE_WRITE; }
  virtual int get_params() = 0;
};

void RGWPSCreateSubOp::execute()
{
  op_ret = get_params();
  if (op_ret < 0) {
    return;
  }
  ups = make_unique<RGWUserPubSub>(store, s->owner.get_id());
  op_ret = ups->add_sub(sub_name, topic_name, dest);
  if (op_ret < 0) {
    ldout(s->cct, 20) << "failed to create subscription, ret=" << op_ret << dendl;
    return;
  }
}

class RGWPSCreateSub_ObjStore_S3 : public RGWPSCreateSubOp {
public:
  explicit RGWPSCreateSub_ObjStore_S3() {}

  int get_params() override {
    sub_name = s->object.name;

    bool exists;

    topic_name = s->info.args.get("topic", &exists);
    if (!exists) {
      ldout(s->cct, 20) << "ERROR: missing required param 'topic' for request" << dendl;
      return -EINVAL;
    }

    dest.push_endpoint = s->info.args.get("push-endpoint");

    return 0;
  }
};

class RGWPSGetSubOp : public RGWOp {
protected:
  string sub_name;
  std::unique_ptr<RGWUserPubSub> ups;
  rgw_pubsub_user_sub_config result;

public:
  RGWPSGetSubOp() {}

  int verify_permission() override {
    return 0;
  }
  void pre_exec() override {
    rgw_bucket_object_pre_exec(s);
  }
  void execute() override;

  const char* name() const override { return "pubsub_subscription_get"; }
  virtual RGWOpType get_type() override { return RGW_OP_PUBSUB_SUB_GET; }
  virtual uint32_t op_mask() override { return RGW_OP_TYPE_READ; }
  virtual int get_params() = 0;
};

void RGWPSGetSubOp::execute()
{
  op_ret = get_params();
  if (op_ret < 0) {
    return;
  }
  ups = make_unique<RGWUserPubSub>(store, s->owner.get_id());
  op_ret = ups->get_sub(sub_name, &result);
  if (op_ret < 0) {
    ldout(s->cct, 20) << "failed to get subscription, ret=" << op_ret << dendl;
    return;
  }
}

class RGWPSGetSub_ObjStore_S3 : public RGWPSGetSubOp {
public:
  explicit RGWPSGetSub_ObjStore_S3() {}

  int get_params() override {
    sub_name = s->object.name;
    return 0;
  }

  void send_response() override {
    if (op_ret) {
      set_req_state_err(s, op_ret);
    }
    dump_errno(s);
    end_header(s, this, "application/json");

    if (op_ret < 0) {
      return;
    }

    {
      Formatter::ObjectSection section(*s->formatter, "result");
      encode_json("topic", result.topic, s->formatter);
      encode_json("push_endpoint", result.dest.push_endpoint, s->formatter);
    }
    rgw_flush_formatter_and_reset(s, s->formatter);
  }
};

class RGWPSDeleteSubOp : public RGWOp {
protected:
  string sub_name;
  string topic_name;
  std::unique_ptr<RGWUserPubSub> ups;

public:
  RGWPSDeleteSubOp() {}

  int verify_permission() override {
    return 0;
  }
  void pre_exec() override {
    rgw_bucket_object_pre_exec(s);
  }
  void execute() override;

  const char* name() const override { return "pubsub_subscription_delete"; }
  virtual RGWOpType get_type() override { return RGW_OP_PUBSUB_SUB_DELETE; }
  virtual uint32_t op_mask() override { return RGW_OP_TYPE_DELETE; }
  virtual int get_params() = 0;
};

void RGWPSDeleteSubOp::execute()
{
  op_ret = get_params();
  if (op_ret < 0) {
    return;
  }
  ups = make_unique<RGWUserPubSub>(store, s->owner.get_id());
  op_ret = ups->remove_sub(sub_name, topic_name);
  if (op_ret < 0) {
    ldout(s->cct, 20) << "failed to remove subscription, ret=" << op_ret << dendl;
    return;
  }
}

class RGWPSDeleteSub_ObjStore_S3 : public RGWPSDeleteSubOp {
public:
  explicit RGWPSDeleteSub_ObjStore_S3() {}

  int get_params() override {
    sub_name = s->object.name;
    topic_name = s->info.args.get("topic");
    return 0;
  }
};

class RGWPSAckSubEventOp : public RGWOp {
protected:
  string sub_name;
  string event_id;
  std::unique_ptr<RGWUserPubSub> ups;

public:
  RGWPSAckSubEventOp() {}

  int verify_permission() override {
    return 0;
  }
  void pre_exec() override {
    rgw_bucket_object_pre_exec(s);
  }
  void execute() override;

  const char* name() const override { return "pubsub_subscription_ack"; }
  virtual RGWOpType get_type() override { return RGW_OP_PUBSUB_SUB_ACK; }
  virtual uint32_t op_mask() override { return RGW_OP_TYPE_WRITE; }
  virtual int get_params() = 0;
};

void RGWPSAckSubEventOp::execute()
{
  op_ret = get_params();
  if (op_ret < 0) {
    return;
  }
  ups = make_unique<RGWUserPubSub>(store, s->owner.get_id());
  op_ret = ups->remove_event(sub_name, event_id);
  if (op_ret < 0) {
    ldout(s->cct, 20) << "failed to remove event, ret=" << op_ret << dendl;
    return;
  }
}

class RGWPSAckSubEvent_ObjStore_S3 : public RGWPSAckSubEventOp {
public:
  explicit RGWPSAckSubEvent_ObjStore_S3() {}

  int get_params() override {
    sub_name = s->object.name;

    bool exists;

    event_id = s->info.args.get("event-id", &exists);
    if (!exists) {
      ldout(s->cct, 20) << "ERROR: missing required param 'event-id' for request" << dendl;
      return -EINVAL;
    }
    return 0;
  }
};

class RGWPSPullSubEventsOp : public RGWOp {
protected:
  int max_entries{0};
  string sub_name;
  string marker;
  std::unique_ptr<RGWUserPubSub> ups;
  RGWUserPubSub::list_events_result result;

public:
  RGWPSPullSubEventsOp() {}

  int verify_permission() override {
    return 0;
  }
  void pre_exec() override {
    rgw_bucket_object_pre_exec(s);
  }
  void execute() override;

  const char* name() const override { return "pubsub_subscription_pull"; }
  virtual RGWOpType get_type() override { return RGW_OP_PUBSUB_SUB_PULL; }
  virtual uint32_t op_mask() override { return RGW_OP_TYPE_READ; }
  virtual int get_params() = 0;
};

void RGWPSPullSubEventsOp::execute()
{
  op_ret = get_params();
  if (op_ret < 0) {
    return;
  }
  ups = make_unique<RGWUserPubSub>(store, s->owner.get_id());
  op_ret = ups->list_events(sub_name, marker, max_entries, &result);
  if (op_ret < 0) {
    ldout(s->cct, 20) << "failed to get subscription, ret=" << op_ret << dendl;
    return;
  }
}

class RGWPSPullSubEvents_ObjStore_S3 : public RGWPSPullSubEventsOp {
public:
  explicit RGWPSPullSubEvents_ObjStore_S3() {}

  int get_params() override {
    sub_name = s->object.name;
    marker = s->info.args.get("marker");
#define DEFAULT_MAX_ENTRIES 100
    int ret = s->info.args.get_int("max-entries", &max_entries, DEFAULT_MAX_ENTRIES);
    if (ret < 0) {
      ldout(s->cct, 20) << "failed to parse 'max-entries' param" << dendl;
      return -EINVAL;
    }
    return 0;
  }

  void send_response() override {
    if (op_ret) {
      set_req_state_err(s, op_ret);
    }
    dump_errno(s);
    end_header(s, this, "application/json");

    if (op_ret < 0) {
      return;
    }

    encode_json("result", result, s->formatter);
    rgw_flush_formatter_and_reset(s, s->formatter);
  }
};

class RGWHandler_REST_PSSub_S3 : public RGWHandler_REST_S3 {
protected:
  bool supports_quota() override {
    return false;
  }
  RGWOp *op_get() override {
    if (s->object.empty()) {
      return nullptr;
    }
    if (s->info.args.exists("events")) {
      return new RGWPSPullSubEvents_ObjStore_S3();
    }
    return new RGWPSGetSub_ObjStore_S3();
  }
  RGWOp *op_put() override {
    if (!s->object.empty()) {
      return new RGWPSCreateSub_ObjStore_S3();
    }
    return nullptr;
  }
  RGWOp *op_delete() override {
    if (!s->object.empty()) {
      return new RGWPSDeleteSub_ObjStore_S3();
    }
    return nullptr;
  }
  RGWOp *op_post() override {
    if (s->info.args.exists("ack")) {
      return new RGWPSAckSubEvent_ObjStore_S3();
    }
    return nullptr;
  }
public:
  explicit RGWHandler_REST_PSSub_S3(const rgw::auth::StrategyRegistry& auth_registry) : RGWHandler_REST_S3(auth_registry) {}
  virtual ~RGWHandler_REST_PSSub_S3() {}
};


RGWHandler_REST* RGWRESTMgr_PubSub_S3::get_handler(struct req_state* const s,
                                                     const rgw::auth::StrategyRegistry& auth_registry,
                                                     const std::string& frontend_prefix)
{
  int ret =
    RGWHandler_REST_S3::init_from_header(s,
					RGW_FORMAT_JSON, true);
  if (ret < 0) {
    return nullptr;
  }

  RGWHandler_REST *handler = nullptr;;

  if (s->init_state.url_bucket == "topics") {
    handler = new RGWHandler_REST_PSTopic_S3(auth_registry);
  }

  if (s->init_state.url_bucket == "subscriptions") {
    handler = new RGWHandler_REST_PSSub_S3(auth_registry);
  }

  ldout(s->cct, 20) << __func__ << " handler=" << (handler ? typeid(*handler).name() : "<null>") << dendl;
  return handler;
}

