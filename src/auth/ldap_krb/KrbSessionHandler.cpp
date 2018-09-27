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

#include "KrbSessionHandler.hpp"
#include "KrbProtocol.hpp"

#include <errno.h>
#include <sstream>

#include "common/config.h"
#include "include/ceph_features.h"
#include "msg/Message.h"
 
#define dout_subsys ceph_subsys_auth


