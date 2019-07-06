// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/socket.h>

#include <netinet/in.h>

#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "common/channel.h"
#include "common/log.h"


/// Obtain the port assigned to the socket during binding. This functions works
/// for both versions of the IP protocol.
/// @return port number
///
/// @param[in]  ch   channel
static uint16_t
get_assigned_port(const struct channel* ch)
{
  int reti;
  struct sockaddr_storage ss;
  struct sockaddr_in* s4;
  struct sockaddr_in6* s6;
  socklen_t len;

  len = sizeof(ss);

  // Request the socket details.
  reti = getsockname(ch->ch_sock, (struct sockaddr*)&ss, &len);
  if (reti == -1) {
    log(LL_WARN, true, "unable to obtain address of the %s socket", ch->ch_name);
    return false;
  }

  // Obtain the address details based on the protocol family.
  if (ss.ss_family == PF_INET) {
    s4 = (struct sockaddr_in*)&ss;
    return s4->sin_port;
  } else {
    s6 = (struct sockaddr_in6*)&ss;
    return s6->sin6_port;
  }
}

/// Reset all channel statistics.
///
/// @param[in] ch channel
static void
reset_stats(struct channel* ch)
{
  ch->ch_rall = 0;
  ch->ch_reni = 0;
  ch->ch_resz = 0;
  ch->ch_remg = 0;
  ch->ch_repv = 0;
  ch->ch_rety = 0;
  ch->ch_sall = 0;
  ch->ch_seni = 0;
}

/// Initialise the local address.
///
/// @param[out] ss   socket address
/// @param[out] len  address length
/// @param[in]  port UDP port
/// @param[in]  ipv4 usage of the IPv4 protocol
static void
init_address(struct sockaddr_storage* ss,
             size_t* len,
             const uint16_t port,
             const bool ipv4)
{
  struct sockaddr_in* s4;
  struct sockaddr_in6* s6;

  (void)memset(ss, 0, sizeof(*ss));

  if (ipv4 == true) {
    *len = sizeof(*s4);
    s4   = (struct sockaddr_in*)ss;

    s4->sin_family      = AF_INET;
    s4->sin_port        = htons(port);
    s4->sin_addr.s_addr = INADDR_ANY;
  } else {
    *len = sizeof(*s6);
    s6   = (struct sockaddr_in6*)ss;

    s6->sin6_family     = AF_INET6;
    s6->sin6_port       = htons(port);
    s6->sin6_addr       = in6addr_any;
  }
}

/// Create a IPv4/IPv6 UDP socket.
/// @return success/failure indication
///
/// @param[in] ch   channel
/// @param[in] ipv4 usage of the IPv4 protocol
static bool
create_socket(struct channel* ch, const bool ipv4)
{
  int pf;

  // Select the appropriate protocol family.
  if (ipv4 == true) {
    pf = PF_INET;
  } else {
    pf = PF_INET6;
  }

  // Create a UDP socket.
  ch->ch_sock = socket(pf, SOCK_DGRAM, IPPROTO_UDP);
  if (ch->ch_sock == -1) {
    log(LL_WARN, true, "unable to initialise the socket");
    return false;
  }

  return true;
}

/// Attach the channel to a local name.
/// @return success/failure indication
///
/// @param[in] ch   channel
/// @param[in] port UDP port number
/// @param[in] ipv4 usage of the IPv4 protocol
static bool
assign_name(struct channel* ch, const uint16_t port, const bool ipv4)
{
  int val;
  int reti;
  struct sockaddr_storage ss;
  size_t len;

  // Set the socket binding to be re-usable.
  val = 1;
  reti = setsockopt(ch->ch_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket address reusable");
    return false;
  }

  // Set the socket to only receive IPv6 traffic.
  if (ipv4 == false) {
    val = 1;
    reti = setsockopt(ch->ch_sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
    if (reti == -1) {
      log(LL_WARN, true, "unable to disable IPv4 traffic on the socket");
      return false;
    }
  }

  // Initialise the appropriate local address.
  init_address(&ss, &len, port, ipv4);

  // Bind the socket to the address.
  reti = bind(ch->ch_sock, (struct sockaddr*)&ss, (socklen_t)len);
  if (reti == -1) {
    log(LL_WARN, true, "unable to bind the socket to port %" PRIu16, port);
    return false;
  }

  // Retrieve the assigned port.
  ch->ch_port = get_assigned_port(ch);

  return true;
}

/// Set the advisory socket buffer sizes.
/// @return success/failure indication
///
/// @param[in] ch   channel
/// @param[in] rbuf receive buffer size in bytes
/// @param[in] sbuf send buffer size in bytes
static bool
set_buffer_sizes(struct channel* ch, const uint64_t rbuf, const uint64_t sbuf)
{
  int val;
  int reti;

  // Set the socket receive buffer size.
  val = (int)rbuf;
  reti = setsockopt(ch->ch_sock, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket receive buffer size to %d", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)sbuf;
  reti = setsockopt(ch->ch_sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket send buffer size to %d", val);
    return false;
  }

  return true;
}

/// Apply time-to-live/hops settings.
/// @return success/failure indication
///
/// @param[in] ch   channel
/// @param[in] ttl  time-to-live/hops value
/// @param[in] ipv4 usage of the IPv4 protocol
static bool
apply_ttl_prefs(struct channel* ch, const uint8_t ttl, const bool ipv4)
{
  int val;
  int reti;
  int lvl;
  int opt_hops;
  int opt_recv;

  // Select appropriate protocol and option identifiers.
  if (ipv4 == true) {
    lvl      = IPPROTO_IP;
    opt_hops = IP_TTL;
    opt_recv = IP_RECVTTL;
  } else {
    lvl      = IPPROTO_IPV6;
    opt_hops = IPV6_UNICAST_HOPS;
    opt_recv = IPV6_RECVHOPLIMIT;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)ttl;
  reti = setsockopt(ch->ch_sock, lvl, opt_hops, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket time-to-live to %d", val);
    return false;
  }

  // Request the ancillary control message for IP TTL value.
  val = 1;
  reti = setsockopt(ch->ch_sock, lvl, opt_recv, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to request time-to-live values on the socket");
    return false;
  }

  return true;
}

/// Create the channel.
/// @return success/failure indication
///
/// @param[in] ch   channel
/// @param[in] ipv4 IPv4 protocol usage
/// @param[in] port UDP port
/// @param[in] rbuf receive buffer size in bytes
/// @param[in] sbuf send buffer size in bytes
/// @param[in] ttl  time-to-live value
bool
open_channel(struct channel* ch,
             const bool ipv4,
             const uint16_t port,
             const uint64_t rbuf,
             const uint64_t sbuf,
             const uint8_t ttl)
{
  int retb;

  if (ipv4 == true) {
    ch->ch_name = "IPv4";
  } else {
    ch->ch_name = "IPv6";
  }

  log(LL_INFO, false, "creating the %s channel", ch->ch_name);

  (void)memset(ch, 0, sizeof(*ch));
  reset_stats(ch);

  // Create a UDP socket.
  retb = create_socket(ch, ipv4);
  if (retb == false) {
    return false;
  }

  // Bind the socket to a local address and port.
  retb = assign_name(ch, port, ipv4);
  if (retb == false) {
    return false;
  }

  // Request buffer sizes from the kernel.
  retb = set_buffer_sizes(ch, rbuf, sbuf);
  if (retb == false) {
    return false;
  }

  // Apply settings that relate to the IP time-to-live value.
  retb = apply_ttl_prefs(ch, ttl, ipv4);
  if (retb == false) {
    return false;
  }

  return true;
}

/// Log all channel information.
///
/// @param[in] ch channel
void
log_channel(const struct channel* ch)
{
  log(LL_DEBUG, false, "local UDP port: %" PRIu16, ch->ch_port);
  log(LL_DEBUG, false, "overall received: %" PRIu64, ch->ch_rall);
  log(LL_DEBUG, false, "receive network-related errors: %" PRIu64, ch->ch_reni);
  log(LL_DEBUG, false, "receive packet size mismatches: %" PRIu64, ch->ch_resz);
  log(LL_DEBUG, false, "receive payload magic mismatches: %" PRIu64, ch->ch_remg);
  log(LL_DEBUG, false, "receive payload version mismatches: %" PRIu64, ch->ch_repv);
  log(LL_DEBUG, false, "receive payload type mismatches: %" PRIu64, ch->ch_rety);
  log(LL_DEBUG, false, "overall sent: %" PRIu64, ch->ch_sall);
  log(LL_DEBUG, false, "send network-related errors: %" PRIu64, ch->ch_seni);
}

/// Close the channel.
///
/// @param[in] ch channel
void
close_channel(const struct channel* ch)
{
  int reti;

  reti = close(ch->ch_sock);
  if (reti == -1) {
    log(LL_WARN, true, "unable to close the %s socket", ch->ch_name);
  }
}
