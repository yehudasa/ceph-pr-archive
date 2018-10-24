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

#ifndef KRB_AUTHORIZE_HANDLER_HPP
#define KRB_AUTHORIZE_HANDLER_HPP

/* Include order and names:
 * a) Immediate related header
 * b) C libraries (if any),
 * c) C++ libraries,
 * d) Other support libraries
 * e) Other project's support libraries
 *
 * Within each section the includes should
 * be ordered alphabetically.
 */

#include "auth/AuthAuthorizeHandler.h"

class KrbAuthorizeHandler : public AuthAuthorizeHandler 
{
  bool verify_authorizer(CephContext*, KeyStore*, 
                         bufferlist&, bufferlist&,
                         EntityName&, uint64_t&, 
                         AuthCapsInfo&, CryptoKey&, 
                         uint64_t* = nullptr, 
                         std::unique_ptr<
                          AuthAuthorizerChallenge>* = nullptr) override;

  int authorizer_session_crypto() override;
  ~KrbAuthorizeHandler() override = default;

};

#endif    //-- KRB_AUTHORIZE_HANDLER_HPP

// ----------------------------- END-OF-FILE --------------------------------//

