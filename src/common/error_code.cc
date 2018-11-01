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

#include <exception>

#include "common/error_code.h"

using boost::system::error_category;
using boost::system::error_condition;
using boost::system::generic_category;

namespace ceph {

// A category for error conditions particular to Ceph

class ceph_error_category : public converting_category {
public:
  ceph_error_category(){}
  const char* name() const noexcept override;
  std::string message(int ev) const noexcept override;
  using converting_category::equivalent;
  bool equivalent(const boost::system::error_code& c,
		  int ev) const noexcept override;
  int from_code(int ev) const noexcept override;
};

const char* ceph_error_category::name() const noexcept {
  return "ceph";
}

std::string ceph_error_category::message(int ev) const noexcept {
  using namespace ::ceph::errc;

  switch (ev) {
  case 0:
    return "No error";

  case not_in_map:
    return "Map does not contain requested entry.";
  }

  return "Unknown error.";
}

bool ceph_error_category::equivalent(const boost::system::error_code& c,
				     int ev) const noexcept {
  if (c.category() == generic_category() &&
      c.value() == boost::system::errc::no_such_file_or_directory &&
      ev == ceph::errc::not_in_map) {
    // Blargh. A bunch of stuff returns ENOENT now, so just to be safe.
    return true;
  }
  return false;
}

int ceph_error_category::from_code(int ev) const noexcept {
  switch (ev) {
  case 0:
    return 0;
  case ceph::errc::not_in_map:
    // What we use now.
    return -ENOENT;
  }
  return -EDOM;
}

const error_category& ceph_category() noexcept {
  static const ceph_error_category c;
  return c;
}


// This is part of the glue for hooking new code to old. Since
// Context* and other things give us integer codes from errno, wrap
// them in an error_code.
boost::system::error_code to_error_code(int ret) noexcept
{
  if (ret < 0)
    return { -ret, boost::system::system_category() };
  else
    return {};
}

// This is more complicated. For the case of categories defined
// elsewhere, we have to convert everything here.
int from_error_code(boost::system::error_code e) noexcept
{
  if (e) {
    auto c = dynamic_cast<const converting_category*>(&e.category());
    // For categories we define
    if (c) {
      return c->from_code(e.value());

    // For categories matching values of errno
    } else if (e.category() == boost::system::system_category() ||
	       e.category() == boost::system::generic_category() ||
	       // ASIO uses the system category for these and matches
	       // system error values.
	       e.category() == boost::asio::error::get_netdb_category() ||
	       e.category() == boost::asio::error::get_addrinfo_category()) {
      return -e.value();

    } else if (e.category() == boost::asio::error::get_misc_category()) {
      // These values are specific to asio
      switch (e.value()) {
      case boost::asio::error::already_open:
	return -EIO;
      case boost::asio::error::eof:
	return -EIO;
      case boost::asio::error::not_found:
	return -ENOENT;
      case boost::asio::error::fd_set_failure:
	return -EINVAL;
      }
    // Add any other categories we use here.
    } else {
      // Marcus likes this as a sentinel for 'Error code? What error code?'
      return -EDOM;
    }
  }
  return 0;
}
}
