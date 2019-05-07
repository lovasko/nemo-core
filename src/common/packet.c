// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <netinet/in.h>

#include <string.h>
#include <inttypes.h>

#include "common/convert.h"
#include "common/packet.h"
#include "common/log.h"


/// Encode the payload to the on-wire format. This function only deals with the
/// multi-byte fields, as all single-byte fields are correctly interpreted on
/// all endian encodings.
///
/// @param[in] dst encoded payload
/// @param[in] src original payload
static void
encode_payload(struct payload* dst, const struct payload* src)
{
  dst->pl_mgic  = htonl(src->pl_mgic);
  dst->pl_port  = htons(src->pl_port);
  dst->pl_snum  = htonll(src->pl_snum);
  dst->pl_slen  = htonll(src->pl_slen);
  dst->pl_laddr = htonll(src->pl_laddr);
  dst->pl_haddr = htonll(src->pl_haddr);
  dst->pl_reqk  = htonll(src->pl_reqk);
  dst->pl_resk  = htonll(src->pl_resk);
  dst->pl_mtm1  = htonll(src->pl_mtm1);
  dst->pl_rtm1  = htonll(src->pl_rtm1);
  dst->pl_mtm2  = htonll(src->pl_mtm2);
  dst->pl_rtm2  = htonll(src->pl_rtm2);
}

/// Decode the on-wire format of the payload. This function only deals with the
/// multi-byte fields, as all single-byte fields are correctly interpreted on
/// all endian encodings.
///
/// @param[in] dst decoded payload
/// @param[in] src original payload
static void
decode_payload(struct payload* dst, const struct payload* src)
{
  dst->pl_mgic  = ntohl(src->pl_mgic);
  dst->pl_port  = ntohs(src->pl_port);
  dst->pl_snum  = ntohll(src->pl_snum);
  dst->pl_slen  = ntohll(src->pl_slen);
  dst->pl_laddr = ntohll(src->pl_laddr);
  dst->pl_haddr = ntohll(src->pl_haddr);
  dst->pl_reqk  = ntohll(src->pl_reqk);
  dst->pl_resk  = ntohll(src->pl_resk);
  dst->pl_mtm1  = ntohll(src->pl_mtm1);
  dst->pl_rtm1  = ntohll(src->pl_rtm1);
  dst->pl_mtm2  = ntohll(src->pl_mtm2);
  dst->pl_rtm2  = ntohll(src->pl_rtm2);
}

/// Verify the incoming payload for correctness.
/// @return success/failure indication
///
/// @param[out] st protocol event statistics 
/// @param[in]  pl payload
static bool
verify_payload(struct stats* st, const struct payload* pl)
{
  log(LL_TRACE, false, "verifying payload");

  // Verify the magic identifier.
  if (pl->pl_mgic != NEMO_PAYLOAD_MAGIC) {
    log(LL_DEBUG, false, "payload identifier unknown, expected: %"
        PRIx32 ", actual: %" PRIx32, NEMO_PAYLOAD_MAGIC, pl->pl_mgic);
    st->st_remg++;
    return false;
  }

  // Verify the payload version.
  if (pl->pl_fver != NEMO_PAYLOAD_VERSION) {
    log(LL_DEBUG, false, "unsupported payload version, expected: %"
        PRIu8 ", actual: %" PRIu8, NEMO_PAYLOAD_VERSION, pl->pl_fver);
    st->st_repv++;
    return false;
  }

  return true;
}

/// Traverse the control messages and obtain the received Time-To-Live value.
///
/// @param[out] ttl Time-To-Live value
/// @param[in]  msg received message
static void
retrieve_ttl(uint8_t* ttl, struct msghdr* msg)
{
  struct cmsghdr* cmsg;
  int type4;
  int type6;
  int val;

  #if defined(__linux__)
    type4 = IP_TTL;
    type6 = IPV6_HOPLIMIT;
  #endif

  #if defined(__FreeBSD__)
    type4 = IP_RECVTTL;
    type6 = IPV6_RECVHOPLIMIT;
  #endif

  for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) {
    // Check the IPv4 label.
    if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == type4) {
      memcpy(&val, CMSG_DATA(cmsg), sizeof(val));
      *ttl = (uint8_t)val;
      return;
    }

    // Check the IPv6 label.
    if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == type6) {
      memcpy(&val, CMSG_DATA(cmsg), sizeof(val));
      *ttl = (uint8_t)val;
      return;
    }
  }

  *ttl = 0;
}

/// Send a payload to a network address.
/// @return success/failure indication
///
/// @param[in] pr   protocol connection
/// @param[in] hpl  payload in host byte order
/// @param[in] addr IPv4/IPv6 address
/// @param[in] err  fail on error
bool
send_packet(struct proto* pr,
            const struct payload* hpl,
            struct sockaddr_storage ad,
            const bool er)
{
  struct payload npl;
  struct msghdr msg;
  struct iovec iov;
  ssize_t len;
  uint8_t lvl;

  log(LL_TRACE, false, "sending a packet");

  // Prepare payload data.
  encode_payload(&npl, hpl);
  (void)memset(&iov, 0, sizeof(iov));
  iov.iov_base = &npl;
  iov.iov_len  = sizeof(npl);

  // Prepare the message.
  (void)memset(&msg, 0, sizeof(msg));
  msg.msg_name       = &ad;
  msg.msg_namelen    = sizeof(ad);
  msg.msg_iov        = &iov;
  msg.msg_iovlen     = 1;
  msg.msg_control    = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags      = 0;

  // Increase the seriousness of the incident in case we are going to fail.
  if (er == true) {
    lvl = LL_WARN;
  } else {
    lvl = LL_DEBUG;
  }

  // Send the UDP datagram in a non-blocking mode.
  pr->pr_stat.st_sall++;
  len = sendmsg(pr->pr_sock, &msg, MSG_DONTWAIT);

  // Verify if sending has completed successfully.
  if (len == -1 || len != (ssize_t)sizeof(npl)) {
    log(lvl, true, "unable to send a payload");
    pr->pr_stat.st_seni++;
    return false;
  }

  return true;
}

/// Receive datagrams on both IPv4 and IPv6.
/// @return success/failure indication
///
/// @param[in]  pr   protocol connection
/// @param[in]  addr IPv4/IPv6 address of the sender
/// @param[out] hpl  payload in host byte order
/// @param[out] npl  payload in network byte order
/// @param[out] ttl  time to live
/// @param[in]  msg  message headers
/// @param[in]  err  exit on error
bool
receive_packet(struct proto* pr,
               struct sockaddr_storage* addr,
               struct payload* hpl,
               struct payload* npl,
							 uint8_t* ttl,
               const bool err)
{
  ssize_t len;
  bool retb;
  uint8_t lvl;
	uint8_t cmsg[256];
  struct msghdr msg;
  struct iovec iov;

  log(LL_TRACE, false, "receiving a packet");

  // Prepare payload data.
  (void)memset(&iov, 0, sizeof(iov));
  iov.iov_base = npl;
  iov.iov_len  = sizeof(*npl);

  // Prepare the message.
  (void)memset(&msg, 0, sizeof(msg));
  msg.msg_name       = addr;
  msg.msg_namelen    = sizeof(*addr);
  msg.msg_iov        = &iov;
  msg.msg_iovlen     = 1;
	msg.msg_control    = cmsg;
	msg.msg_controllen = sizeof(cmsg);
	msg.msg_flags      = 0;

  // Increase the seriousness of the incident in case we are going to fail.
  if (err == true) {
    lvl = LL_WARN;
  } else {
    lvl = LL_DEBUG;
  }

  // Receive the message and handle potential errors.
  pr->pr_stat.st_rall++;
  len = recvmsg(pr->pr_sock, &msg, MSG_DONTWAIT | MSG_TRUNC);

	// Check for errors during the receipt.
  if (len < 0) {
    log(lvl, true, "receiving has failed");
    pr->pr_stat.st_reni++;
    return false;
  }

	// Check for received packet payload size.
	if (len != (ssize_t)sizeof(*npl) || msg.msg_flags & MSG_TRUNC) {
	  log(lvl, false, "wrong payload size, expected %zd, actual %zu", len, sizeof(*npl));
    pr->pr_stat.st_resz++;
    return false;
	}

	// Check for received packet control payload size.
	if (msg.msg_flags & MSG_CTRUNC) {
	  log(LL_DEBUG, false, "control message truncated");
	}

  // Convert the payload from its on-wire format.
  decode_payload(hpl, npl);

	// Obtain the TTL/hops value.
	retrieve_ttl(ttl, &msg);

  // Verify the payload correctness.
  retb = verify_payload(&pr->pr_stat, hpl);
  if (retb == false) {
    log(LL_WARN, false, "invalid payload");
    return false;
  }

  return true;
}
