// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <netinet/in.h>

#include <stdlib.h>

#include "util/ttl.h"


/// Traverse the control messages and obtain the received Time-To-Live value.
/// @return status code
///
/// @param[out] ttl Time-To-Live value
/// @param[in]  msg received message
bool
retrieve_ttl(int* ttl, struct msghdr* msg)
{
  struct cmsghdr* cmsg;
  int type;

  #if defined(__linux__)
    type = IP_TTL;
  #endif

  #if defined(__FreeBSD__)
    type = IP_RECVTTL;
  #endif

  for (cmsg = CMSG_FIRSTHDR(msg);
       cmsg != NULL;
       cmsg = CMSG_NXTHDR(msg, cmsg)) {
    if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == type) {
      *ttl = *(int*)CMSG_DATA(cmsg);
      return true;
    }
  }

  *ttl = -1;
  return false;
}
