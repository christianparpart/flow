// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/util/Cidr.h>
#include <flow/util/IPAddress.h>

namespace flow::util {

std::string Cidr::str() const {
  char result[INET6_ADDRSTRLEN + 32];

  inet_ntop(static_cast<int>(ipaddr_.family()), ipaddr_.data(), result, sizeof(result));

  size_t n = strlen(result);
  snprintf(result + n, sizeof(result) - n, "/%zu", prefix_);

  return result;
}

bool Cidr::contains(const IPAddress& ipaddr) const {
  if (ipaddr.family() != address().family()) return false;

  // IPv4
  if (ipaddr.family() == IPAddress::Family::V4) {
    uint32_t ip = *(uint32_t*)ipaddr.data();
    uint32_t subnet = *(uint32_t*)address().data();
    uint32_t match = ip & (0xFFFFFFFF >> (32 - prefix()));

    return match == subnet;
  }

  // IPv6
  int bits = prefix();
  const uint32_t* words = (const uint32_t*)address().data();
  const uint32_t* input = (const uint32_t*)ipaddr.data();
  while (bits >= 32) {
    uint32_t match = *words & 0xFFFFFFFF;
    if (match != *input) return false;

    words++;
    input++;
    bits -= 32;
  }

  uint32_t match = *words & 0xFFFFFFFF >> (32 - bits);
  if (match != *input) return false;

  return true;
}

bool operator==(const Cidr& a, const Cidr& b) {
  if (&a == &b)
    return true;

  return a.prefix_ == b.prefix_ && a.ipaddr_ == b.ipaddr_;
}

bool operator!=(const Cidr& a, const Cidr& b) { return !(a == b); }

}  // namespace flow::util
