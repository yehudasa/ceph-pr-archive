// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_RGW_TOOLS_H
#define CEPH_RGW_TOOLS_H

#include <string>

#include "include/types.h"
#include "common/ceph_time.h"
#include "rgw_common.h"

class RGWRados;
class RGWObjectCtx;
struct RGWObjVersionTracker;

struct obj_version;

int rgw_put_system_obj(RGWRados *rgwstore, const rgw_pool& pool, const string& oid, const bufferlist& data, bool exclusive,
                       RGWObjVersionTracker *objv_tracker, real_time set_mtime, map<string, bufferlist> *pattrs = NULL);
int rgw_get_system_obj(RGWRados *rgwstore, RGWObjectCtx& obj_ctx, const rgw_pool& pool, const string& key, bufferlist& bl,
                       RGWObjVersionTracker *objv_tracker, real_time *pmtime, map<string, bufferlist> *pattrs = NULL,
                       rgw_cache_entry_info *cache_info = NULL,
		       boost::optional<obj_version> refresh_version = boost::none);
int rgw_delete_system_obj(RGWRados *rgwstore, const rgw_pool& pool, const string& oid,
                          RGWObjVersionTracker *objv_tracker);

int rgw_tools_init(CephContext *cct);
void rgw_tools_cleanup();
const char *rgw_find_mime_by_ext(string& ext);

template<class H, size_t S>
class RGWEtag
{
  H hash;

public:
  RGWEtag() {}

  void update(const char *buf, size_t len) {
    hash.Update((const unsigned char *)buf, len);
  }

  void update(bufferlist& bl) {
    if (bl.length() > 0) {
      update(bl.c_str(), bl.length());
    }
  }

  void update(const string& s) {
    if (!s.empty()) {
      update(s.c_str(), s.size());
    }
  }
  void finish(string *etag) {
    char etag_buf[S];
    char etag_buf_str[S * 2 + 16];

    hash.Final((unsigned char *)etag_buf);
    buf_to_hex((const unsigned char *)etag_buf, S,
	       etag_buf_str);

    *etag = etag_buf_str;
  }
};

using RGWMD5Etag = RGWEtag<MD5, CEPH_CRYPTO_MD5_DIGESTSIZE>;

class RGWDataAccess
{
  RGWRados *store;
  std::unique_ptr<RGWObjectCtx> obj_ctx;

public:
  RGWDataAccess(RGWRados *_store);

  class Object;
  class Bucket;

  using BucketRef = std::shared_ptr<Bucket>;
  using ObjectRef = std::shared_ptr<Object>;

  class Bucket {
    friend class RGWDataAccess;
    friend class Object;

    std::shared_ptr<Bucket> self_ref;

    RGWDataAccess *sd{nullptr};
    RGWBucketInfo bucket_info;
    string tenant;
    string name;
    string bucket_id;
    ceph::real_time mtime;
    map<std::string, bufferlist> attrs;

    RGWAccessControlPolicy policy;
    int finish_init();
    
    Bucket(RGWDataAccess *_sd,
	   const string& _tenant,
	   const string& _name,
	   const string& _bucket_id) : sd(_sd),
                                       tenant(_tenant),
                                       name(_name),
				       bucket_id(_bucket_id) {}
    Bucket(RGWDataAccess *_sd) : sd(_sd) {}
    int init();
    int init(const RGWBucketInfo& _bucket_info, const map<string, bufferlist>& _attrs);
  public:
    int get_object(const rgw_obj_key& key,
		   ObjectRef *obj);

  };


  class Object {
    RGWDataAccess *sd{nullptr};
    BucketRef bucket;
    rgw_obj_key key;

    ceph::real_time mtime;
    string etag;
    std::optional<uint64_t> olh_epoch;
    ceph::real_time delete_at;

    std::optional<bufferlist> aclbl;

    Object(RGWDataAccess *_sd,
           BucketRef& _bucket,
           const rgw_obj_key& _key) : sd(_sd),
                                      bucket(_bucket),
                                      key(_key) {}
  public:
    int put(bufferlist& data, map<string, bufferlist>& attrs); /* might modify attrs */

    void set_mtime(const ceph::real_time& _mtime) {
      mtime = _mtime;
    }

    void set_etag(const string& _etag) {
      etag = _etag;
    }

    void set_olh_epoch(uint64_t epoch) {
      olh_epoch = epoch;
    }

    void set_delete_at(ceph::real_time _delete_at) {
      delete_at = _delete_at;
    }

    void set_policy(const RGWAccessControlPolicy& policy);

    friend class Bucket;
  };

  int get_bucket(const string& tenant,
		 const string name,
		 const string bucket_id,
		 BucketRef *bucket) {
    bucket->reset(new Bucket(this, tenant, name, bucket_id));
    (*bucket)->self_ref = *bucket;
    return (*bucket)->init();
  }

  int get_bucket(const RGWBucketInfo& bucket_info,
		 const map<string, bufferlist>& attrs,
		 BucketRef *bucket) {
    bucket->reset(new Bucket(this));
    (*bucket)->self_ref = *bucket;
    return (*bucket)->init(bucket_info, attrs);
  }
  friend class Bucket;
  friend class Object;
};

#endif
