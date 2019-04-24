// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/socket.h>

#include <netinet/in.h>

#include <unistd.h>
#include <string.h>

#include "common/log.h"
#include "ures/funcs.h"
#include "ures/types.h"


/// Create the IPv4 socket.
/// @return success/failure indication
///
/// @param[out] pr protocol connection
/// @param[in]  cf configuration
bool
create_socket4(struct proto* pr, const struct config* cf)
{
  int reti;
  struct sockaddr_in addr;
  int val;

  log(LL_INFO, false, "creating %s socket", "IPv4");

  // Create a UDP socket.
  pr->pr_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (pr->pr_sock == -1) {
    log(LL_WARN, true, "unable to initialise the socket");
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket address reusable");
    return false;
  }

  // Bind the socket to the selected port and local address.
  (void)memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons((uint16_t)cf->cf_port);
  addr.sin_addr.s_addr = INADDR_ANY;
  reti = bind(pr->pr_sock, (struct sockaddr*)&addr, sizeof(addr));
  if (reti == -1) {
    log(LL_WARN, true, "unable to bind the socket");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)cf->cf_rbuf;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket receive buffer size to %d", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)cf->cf_sbuf;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket send buffer size to %d", val);
    return false;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)cf->cf_ttl;
  reti = setsockopt(pr->pr_sock, IPPROTO_IP, IP_TTL, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket Time-To-Live to %d",
      val);
    return false;
  }

  // Request the ancillary control message for IP TTL value.
  val = 1;
  reti = setsockopt(pr->pr_sock, IPPROTO_IP, IP_RECVTTL, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to request Time-To-Live values on the socket");
    return false;
  }

  return true;
}

/// Create the IPv6 socket.
/// @return success/failure indication
///
/// @param[out] pr protocol connection
/// @param[in]  cf configuration
bool
create_socket6(struct proto* pr, const struct config* cf)
{
  int reti;
  struct sockaddr_in6 addr;
  int val;

  log(LL_INFO, false, "creating %s socket", "IPv6");

  // Create a UDP socket.
  pr->pr_sock = socket(AF_INET6, SOCK_DGRAM, 0);
  if (pr->pr_sock == -1) {
    log(LL_WARN, true, "unable to initialize the socket");
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket address reusable");
    return false;
  }

  // Set the socket to only receive IPv6 traffic.
  val = 1;
  reti = setsockopt(pr->pr_sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to disable IPv4 traffic on the socket");
    return false;
  }

  // Bind the socket to the selected port and local address.
  (void)memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_port   = htons((uint16_t)cf->cf_port);
  addr.sin6_addr   = in6addr_any;
  reti = bind(pr->pr_sock, (struct sockaddr*)&addr, sizeof(addr));
  if (reti == -1) {
    log(LL_WARN, true, "unable to bind the socket");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)cf->cf_rbuf;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the receive buffer size to %d", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)cf->cf_sbuf;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the send buffer size to %d", val);
    return false;
  }

  // Set the outgoing hop count.
  val = (int)cf->cf_ttl;
  reti = setsockopt(pr->pr_sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set hop count to %d", val);
    return false;
  }

  // Request the ancillary control message for hop counts.
  val = 1;
  reti = setsockopt(pr->pr_sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to request hop count values");
    return false;
  }

  return true;
}

/// Delete the protocol socket.
///
/// @param[in] pr protocol connection
void
delete_socket(const struct proto* pr)
{
  int reti;

  reti = close(pr->pr_sock);
  if (reti == -1) {
    log(LL_WARN, true, "unable to close the %s socket", pr->pr_name);
  }
}
