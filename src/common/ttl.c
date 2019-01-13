// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <netinet/in.h>

#include <stdlib.h>
#include <string.h>

#include "common/ttl.h"


/// Traverse the control messages and obtain the received Time-To-Live value.
/// @return status code
///
/// @param[out] ttl Time-To-Live value
/// @param[in]  msg received message
bool
retrieve_ttl(int* ttl, struct msghdr* msg)
{
  struct cmsghdr* cmsg;
  int type4;
  int type6;

  #if defined(__linux__)
    type4 = IP_TTL;
    type6 = IPV6_HOPLIMIT;
  #endif

  #if defined(__FreeBSD__)
    type4 = IP_RECVTTL;
    type6 = IPV6_RECVHOPLIMIT;
  #endif

  for (cmsg = CMSG_FIRSTHDR(msg);
       cmsg != NULL;
       cmsg = CMSG_NXTHDR(msg, cmsg)) {
    // Check the IPv4 label.
    if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == type4) {
      memcpy(ttl, CMSG_DATA(cmsg), sizeof(*ttl));
      return true;
    }

    // Check the IPv6 label.
    if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == type6) {
      memcpy(ttl, CMSG_DATA(cmsg), sizeof(*ttl));
      return true;
    }
  }

  *ttl = -1;
  return false;
}
