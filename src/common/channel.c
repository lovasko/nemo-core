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
/// @return success/failure indication
///
/// @param[out] port port number
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

/// Create the IPv4 channel.
/// @return success/failure indication
///
/// @param[in] ch   channel
/// @param[in] port UDP port
/// @param[in] rbuf receive buffer size in bytes
/// @param[in] sbuf send buffer size in bytes
/// @param[in] ttl  time-to-live value
bool
open_channel4(struct channel* ch,
              const uint16_t port,
              const uint64_t rbuf,
              const uint64_t sbuf,
              const uint8_t ttl)
{
  int reti;
  struct sockaddr_in addr;
  int val;

  log(LL_INFO, false, "creating the %s channel", "IPv4");

  reset_stats(ch);
  ch->ch_name = "IPv4";
  (void)memset(ch->ch_pad, 0, sizeof(ch->ch_pad));

  // Create a UDP socket.
  ch->ch_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (ch->ch_sock == -1) {
    log(LL_WARN, true, "unable to initialise the socket");
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  reti = setsockopt(ch->ch_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket address reusable");
    return false;
  }

  // Bind the socket to the selected port and local address.
  (void)memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  reti = bind(ch->ch_sock, (struct sockaddr*)&addr, sizeof(addr));
  if (reti == -1) {
    log(LL_WARN, true, "unable to bind the socket to port %" PRIu16, port);
    return false;
  }
  ch->ch_port = get_assigned_port(ch);

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

  // Set the outgoing Time-To-Live value.
  val = (int)ttl;
  reti = setsockopt(ch->ch_sock, IPPROTO_IP, IP_TTL, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket time-to-live to %d", val);
    return false;
  }

  // Request the ancillary control message for IP TTL value.
  val = 1;
  reti = setsockopt(ch->ch_sock, IPPROTO_IP, IP_RECVTTL, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to request time-to-live values on the socket");
    return false;
  }

  return true;
}

/// Create the IPv6 channel.
/// @return success/failure indication
///
/// @param[in] ch   channel
/// @param[in] port UDP port
/// @param[in] rbuf receive buffer size in bytes
/// @param[in] sbuf send buffer size in bytes
/// @param[in] hops time-to-live value
bool
open_channel6(struct channel* ch,
              const uint16_t port,
              const uint64_t rbuf,
              const uint64_t sbuf,
              const uint8_t hops)
{
  int reti;
  struct sockaddr_in6 addr;
  int val;

  log(LL_INFO, false, "creating the %s channel", "IPv6");

  // Create a UDP socket.
  ch->ch_sock = socket(AF_INET6, SOCK_DGRAM, 0);
  if (ch->ch_sock == -1) {
    log(LL_WARN, true, "unable to initialize the socket");
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  reti = setsockopt(ch->ch_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the socket address reusable");
    return false;
  }

  // Set the socket to only receive IPv6 traffic.
  val = 1;
  reti = setsockopt(ch->ch_sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to disable IPv4 traffic on the socket");
    return false;
  }

  // Bind the socket to the selected port and local address.
  (void)memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_port   = htons(port);
  addr.sin6_addr   = in6addr_any;
  reti = bind(ch->ch_sock, (struct sockaddr*)&addr, sizeof(addr));
  if (reti == -1) {
    log(LL_WARN, true, "unable to bind the socket");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)rbuf;
  reti = setsockopt(ch->ch_sock, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the receive buffer size to %d", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)sbuf;
  reti = setsockopt(ch->ch_sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set the send buffer size to %d", val);
    return false;
  }

  // Set the outgoing hop count.
  val = (int)hops;
  reti = setsockopt(ch->ch_sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to set hop count to %d", val);
    return false;
  }

  // Request the ancillary control message for hop counts.
  val = 1;
  reti = setsockopt(ch->ch_sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &val, sizeof(val));
  if (reti == -1) {
    log(LL_WARN, true, "unable to request hop count values");
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
