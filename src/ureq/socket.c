// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/socket.h>

#include <netinet/in.h>

#include <string.h>

#include "common/log.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


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

  log(LL_INFO, false, "main", "creating %s socket", pr->pr_name);

  // Create a UDP socket.
  pr->pr_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (pr->pr_sock == -1) {
    log(LL_WARN, true, "main", "unable to initialise the socket");
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the socket address reusable");
    return false;
  }

  // Bind the socket to the selected port and local address.
  (void)memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  reti = bind(pr->pr_sock, (struct sockaddr*)&addr, sizeof(addr));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to bind the socket");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)cf->cf_rbuf;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the socket receive buffer size "
      "to %d", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)cf->cf_sbuf;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the socket send buffer size"
      "to %d", val);
    return false;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)cf->cf_ttl;
  reti = setsockopt(pr->pr_sock, IPPROTO_IP, IP_TTL, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the socket time-to-live to %d",
      val);
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

  log(LL_INFO, false, "main", "creating %s socket", pr->pr_name);

  // Create a UDP socket.
  pr->pr_sock = socket(AF_INET6, SOCK_DGRAM, 0);
  if (pr->pr_sock == -1) {
    log(LL_WARN, true, "main", "unable to initialize the socket");
    return false;
  }

  // Set the socket binding to be re-usable. This has to happen before binding
  // the socket.
  val = 1;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the socket address reusable");
    return false;
  }

  // Set the socket to only receive IPv6 traffic. This has to happen before
  // binding the socket.
  val = 1;
  reti = setsockopt(pr->pr_sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to disable IPv4 traffic on the socket");
    return false;
  }

  // Bind the socket to the selected port and local address.
  (void)memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_addr   = in6addr_any;
  reti = bind(pr->pr_sock, (struct sockaddr*)&addr, sizeof(addr));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to bind the socket");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)cf->cf_rbuf;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the socket receive buffer size "
      "to %d", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)cf->cf_sbuf;
  reti = setsockopt(pr->pr_sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set the socket send buffer size to "
      "%d", val);
    return false;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)cf->cf_ttl;
  reti = setsockopt(pr->pr_sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val,
    sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to set time-to-live to %d", val);
    return false;
  }

  return true;
}

/// Obtain the port assigned to the socket during binding. This functions works
/// for both versions of the IP protocol.
/// @return sucess/failure indication
///
/// @param[out] pn port number
/// @param[in]  pr protocol connection
bool
get_assigned_port(uint16_t* pn, const struct proto* pr)
{
  int reti;
  struct sockaddr_storage ss;
  struct sockaddr_in* s4;
  struct sockaddr_in6* s6;
  socklen_t len;

  len = sizeof(ss);

  // Request the socket details.
  reti = getsockname(pr->pr_sock, (struct sockaddr*)&ss, &len);
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to obtain address of the %s socket",
      pr->pr_name);
    return false;
  }
  
  // Assign an invalid number as the result.
  *pn = 0;

  // Obtain the address details based on the protocol family.
  if (ss.ss_family == PF_INET) {
    s4 = (struct sockaddr_in*)&ss;
    *pn = s4->sin_port;
  } 
  if (ss.ss_family == PF_INET6) {
    s6 = (struct sockaddr_in6*)&ss;
    *pn = s6->sin6_port;
  }

  // Verify that the result is valid.
  if (*pn == 0) {
    log(LL_WARN, false, "main", "unable to retrieve the port number");
    return false;
  }

  return true;
}
