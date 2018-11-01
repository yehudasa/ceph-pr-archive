// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2017 Red Hat, Inc. <contact@redhat.com>
 *
 * Author: Adam C. Emerson <aemerson@redhat.com>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version
 * 2.1, as published by the Free Software Foundation.  See file
 * COPYING.
 */

#include <netdb.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>

namespace ceph {

// This is for error categories we define, so we can specify the
// equivalent integral value at the point of definition.
class converting_category : public boost::system::error_category {
public:
  virtual int from_code(int code) const noexcept = 0;
};

const boost::system::error_category& ceph_category() noexcept;

namespace errc {
enum ceph_errc_t {
  not_in_map = 1 // The requested item was not found in the map
};
}
}

namespace boost {
namespace system {
template<>
struct is_error_condition_enum<::ceph::errc::ceph_errc_t> {
  static const bool value = true;
};
}
}

namespace ceph {
namespace errc {
//  explicit conversion:
inline boost::system::error_code make_error_code(ceph_errc_t e) noexcept {
  return { e, ceph_category() };
}

// implicit conversion:
inline boost::system::error_condition make_error_condition(ceph_errc_t e)
  noexcept {
  return { e, ceph_category() };
}
}

boost::system::error_code to_error_code(int ret) noexcept;
int from_error_code(boost::system::error_code e) noexcept;
}
