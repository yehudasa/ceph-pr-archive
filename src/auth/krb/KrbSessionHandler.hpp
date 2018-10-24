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

#ifndef KRB_SESSION_HANDLER_HPP
#define KRB_SESSION_HANDLER_HPP

#include "auth/AuthSessionHandler.h"
#include "auth/Auth.h"

class CephContext;
class Message;

class KrbSessionHandler : public AuthSessionHandler {

  public:
    KrbSessionHandler(CephContext* ceph_ctx, CryptoKey session_key) : 
        AuthSessionHandler(ceph_ctx, CEPH_AUTH_GSS, session_key) { }
    ~KrbSessionHandler() override = default; 

    bool no_security() override { return true; }
    int sign_message(Message* msg) override { return 0; }
    int check_message_signature(Message* msg) override { return 0; }
    int encrypt_message(Message* msg) override { return 0; }
    int decrypt_message(Message* msg) override { return 0; }

  private:
};

#endif    //-- KRB_SESSION_HANDLER_HPP


