// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2017 Daniel Oliveira <doliveira@suse.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#ifndef KRB_CLIENT_HANDLER_HPP
#define KRB_CLIENT_HANDLER_HPP

#include <map>

#include "auth/Auth.h"
#include "auth/AuthClientHandler.h"
#include "auth/RotatingKeyRing.h"

#include "KrbProtocol.hpp"
#include "common/ceph_context.h"
#include "common/config.h"
#include "ceph_krb_auth.hpp"

class CephContext;
class Keyring;

using map_string_all_t = std::map<std::string, std::string>; 

template<LockPolicy lock_policy> 
class KrbClientHandler : public AuthClientHandler
{

  public:
    KrbClientHandler(CephContext* ceph_ctx = nullptr, 
                     RotatingKeyRing<lock_policy>* key_secrets = nullptr) 
      : AuthClientHandler(ceph_ctx),
      m_gss_client_name(GSS_C_NO_NAME), 
      m_gss_service_name(GSS_C_NO_NAME), 
      m_gss_credentials(GSS_C_NO_CREDENTIAL), 
      m_gss_sec_ctx(GSS_C_NO_CONTEXT), 
      m_gss_buffer_out({0})
    {
      reset();
    }
    ~KrbClientHandler() override;
    
    int get_protocol() const override { return CEPH_AUTH_GSS; }
    void reset() override
    {
      m_gss_client_name = GSS_C_NO_NAME; 
      m_gss_service_name = GSS_C_NO_NAME; 
      m_gss_credentials = GSS_C_NO_CREDENTIAL;
      m_gss_sec_ctx = GSS_C_NO_CONTEXT;
      m_gss_buffer_out = {0};
    }

    void prepare_build_request() override { }
    int build_request(bufferlist&) const override;
    int handle_response(int, bufferlist::const_iterator&) override;
    bool build_rotating_request(bufferlist&) const override { return false; }
    AuthAuthorizer* build_authorizer(uint32_t) const override;
    bool need_tickets() override { return false; }

    void set_global_id(uint64_t guid) override {
      global_id = guid;
    }


  private:
    gss_name_t m_gss_client_name; 
    gss_name_t m_gss_service_name; 
    gss_cred_id_t m_gss_credentials; 
    gss_ctx_id_t m_gss_sec_ctx; 
    gss_buffer_desc m_gss_buffer_out; 

  protected:
    void validate_tickets() override { } 
};


class CephGSSCCache 
{
  public: 
    CephGSSCCache() = default;
    ~CephGSSCCache() = default;
    std::string m_filename{}; 
    map_string_all_t m_environ_vars{};
    void* m_data{nullptr};
};

class CephGSSMechanism 
{
  public: 
    CephGSSMechanism() = default; 
    ~CephGSSMechanism() = default;
    std::string m_enc_name{};
    std::string m_mech_name{};
    gss_OID_desc m_gss_oid = {GSS_API_SPNEGO_OID_PTR}; 
};

class CephGSSClient 
{
  public: 
    CephGSSClient() = default; 
    ~CephGSSClient() = default;
    gss_buffer_desc m_display_name = {0,0};
    gss_buffer_desc m_export_name = {0,0};
    gss_cred_id_t m_credentials = GSS_C_NO_CREDENTIAL;
    CephGSSCCache m_store;
    std::unique_ptr<CephGSSMechanism> m_gss_mech{nullptr};
};

class CephGSSContext
{
  public: 
    CephGSSContext() = default;
    ~CephGSSContext() = default;
    OM_uint32 major_status{0};    /* Used by both */
    OM_uint32 minor_status{0};    /* Used by both */
    gss_ctx_id_t gss_context = {GSS_C_NO_CONTEXT};    /* Used by both */
    gss_name_t gss_service_name = {GSS_C_NO_NAME};    /* Used by both */
    gss_name_t gss_client_name = {GSS_C_NO_NAME};     /* Used by server */
    gss_OID gss_client_type = {};     /* Used by client */ 
    gss_cred_id_t m_client_credentials = GSS_C_NO_CREDENTIAL;     /* Used by server */
};


#endif    //-- KRB_CLIENT_HANDLER_HPP


