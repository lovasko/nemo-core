// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/socket.h>

#include <netinet/in.h>

#include "res/proto.h"
#include "res/types.h"
#include "util/log.h"


/// Create the IPv4 socket.
/// @return success/failure indication
///
/// @param[out] sock socket object
/// @param[in]  opts command-line options
bool
create_socket4(int* sock, const struct options* opts)
{
  int reti;
  struct sockaddr_in addr;
  int val;

  // Early exit if IPv4 is not selected.
  if (opts->op_ipv4 == false)
    return true;

  log(LL_INFO, false, "main", "creating %s socket", "IPv4");

  // Create a UDP socket.
  *sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (*sock < 0) {
    log(LL_WARN, true, "main", "unable to initialise the %s socket", "IPv4");
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  reti = setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket address reusable",
        "IPv4");
    return false;
  }

  // Bind the socket to the selected port and local address.
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons((uint16_t)opts->op_port);
  addr.sin_addr.s_addr = INADDR_ANY;
  reti = bind(*sock, (struct sockaddr*)&addr, sizeof(addr));
  if (reti < 0) {
    log(LL_WARN, true, "main", "unable to bind the %s socket", "IPv4");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)opts->op_rbuf;
  reti = setsockopt(*sock, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket receive buffer "
      "size to %d", "IPv4", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)opts->op_sbuf;
  reti = setsockopt(*sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket send buffer size "
      "to %d", "IPv4", val);
    return false;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)opts->op_ttl;
  reti = setsockopt(*sock, IPPROTO_IP, IP_TTL, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket time-to-live to "
      "%d", "IPv4", val);
    return false;
  }

  // Request the ancillary control message for IP TTL value.
  val = 1;
  reti = setsockopt(*sock, IPPROTO_IP, IP_RECVTTL, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to request time-to-live values on "
      "%s socket", "IPv4", val);
    return false;
  }

  return true;
}

/// Create the IPv6 socket.
/// @return success/failure indication
///
/// @param[out] sock socket object
/// @param[in]  opts command-line options
bool
create_socket6(int* sock, const struct options* opts)
{
  int reti;
  struct sockaddr_in6 addr;
  int val;

  // Early exit if IPv6 is not selected.
  if (opts->op_ipv6 == false)
    return true;

  log(LL_INFO, false, "main", "creating %s socket", "IPv6");

  // Create a UDP socket.
  *sock = socket(AF_INET6, SOCK_DGRAM, 0);
  if (*sock < 0) {
    log(LL_WARN, true, "main", "unable to initialize the %s socket", "IPv6");
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  reti = setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket address reusable", "IPv6");
    return false;
  }

  // Set the socket to only receive IPv6 traffic.
  val = 1;
  reti = setsockopt(*sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to disable IPv4 traffic on %s socket", "IPv6");
    return false;
  }

  // Bind the socket to the selected port and local address.
  addr.sin6_family = AF_INET6;
  addr.sin6_port   = htons((uint16_t)opts->op_port);
  addr.sin6_addr   = in6addr_any;
  reti = bind(*sock, (struct sockaddr*)&addr, sizeof(addr));
  if (reti < 0) {
    log(LL_WARN, true, "main", "unable to bind the %s socket", "IPv6");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)opts->op_rbuf;
  reti = setsockopt(*sock, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the read socket buffer size to %d", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)opts->op_sbuf;
  reti = setsockopt(*sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the socket send buffer size to %d", val);
    return false;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)opts->op_ttl;
  reti = setsockopt(*sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set time-to-live to %d", val);
    return false;
  }

  return true;
}
