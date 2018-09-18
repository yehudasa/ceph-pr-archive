// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (c) 2018 SUSE LLC.
 * Author: Daniel Oliveira <doliveira@suse.com>
 * 
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#ifndef KRB_PROTOCOL_HPP
#define KRB_PROTOCOL_HPP

#include "auth/Auth.h"
#include "ceph_krb_auth.hpp"

#include <errno.h>

#include <sstream> 
#include <string>


struct AuthAuthorizer;

class KrbAuthorizer : public AuthAuthorizer 
{
  public:
    KrbAuthorizer() : AuthAuthorizer(CEPH_AUTH_GSS) { }
    ~KrbAuthorizer() = default; 
    bool build_authorizer(const EntityName& entity_name, 
                          const uint64_t guid) 
    {
      uint8_t value = (1);

      using ceph::encode;
      encode(value, bl, 0);
      encode(entity_name, bl, 0); 
      encode(guid, bl, 0);
      return false;
    }

    bool verify_reply(bufferlist::const_iterator& 
                      buff_list) override { return true; }
    bool add_challenge(CephContext*, 
                       bufferlist&) override { return true; }
};

class KrbRequest
{
  public:
    void decode(bufferlist::const_iterator& buff_list) 
    {
      using ceph::decode;
      decode(m_request_type, buff_list);
    }

    void encode(bufferlist& buff_list) const 
    {
      using ceph::encode;
      encode(m_request_type, buff_list);
    }

    uint16_t m_request_type; 
};
WRITE_CLASS_ENCODER(KrbRequest);

class KrbResponse
{
  public: 
    void decode(bufferlist::const_iterator& buff_list) 
    {
      using ceph::decode;
      decode(m_response_type, buff_list); 
    }    

    void encode(bufferlist& buff_list) const
    {
      using ceph::encode;
      encode(m_response_type, buff_list); 
    }

    uint16_t m_response_type;
};
WRITE_CLASS_ENCODER(KrbResponse);

class KrbTokenBlob 
{
  public:
    void decode(bufferlist::const_iterator& buff_list) 
    {
      uint8_t value = (0); 
     
      using ceph::decode; 
      decode(value, buff_list);
      decode(m_token_blob, buff_list);
    }
        
    void encode(bufferlist& buff_list) const
    {
      uint8_t value = (1); 
      
      using ceph::encode;
      encode(value, buff_list, 0);
      encode(m_token_blob, buff_list, 0);
    }

    bufferlist m_token_blob;
};
WRITE_CLASS_ENCODER(KrbTokenBlob);

std::string gss_auth_show_status(const OM_uint32, const OM_uint32); 

#endif    //-- KRB_PROTOCOL_HPP


