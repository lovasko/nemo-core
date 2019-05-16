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


// This memory block is used to send and receive packets that are larger than
// diagnostic payload.
static uint8_t wrapper[65536];

/// Encode the payload to the on-wire format.
///
/// @param[in] dst encoded payload
/// @param[in] src original payload
static void
encode_payload(struct payload* dst, const struct payload* src)
{
  // Copy the whole payload. This ensures that all single-byte fields are
  // copied correctly, future-proofing the code for future field additions.
  (void)memcpy(dst, src, sizeof(*src));

  // Handle all multi-byte conversions into the network byte order.
  dst->pl_mgic  = htonl(src->pl_mgic);
  dst->pl_port  = htons(src->pl_port);
  dst->pl_len   = htons(src->pl_len);
  dst->pl_snum  = htonll(src->pl_snum);
  dst->pl_slen  = htonll(src->pl_slen);
  dst->pl_laddr = htonll(src->pl_laddr);
  dst->pl_haddr = htonll(src->pl_haddr);
  dst->pl_key   = htonll(src->pl_key);
  dst->pl_mtm1  = htonll(src->pl_mtm1);
  dst->pl_rtm1  = htonll(src->pl_rtm1);
  dst->pl_mtm2  = htonll(src->pl_mtm2);
  dst->pl_rtm2  = htonll(src->pl_rtm2);
}

/// Decode the on-wire format of the payload.
///
/// @param[in] dst decoded payload
/// @param[in] src original payload
static void
decode_payload(struct payload* dst, const struct payload* src)
{
  // Copy the whole payload. This ensures that all single-byte fields are
  // copied correctly, future-proofing the code for future field additions.
  (void)memcpy(dst, src, sizeof(*src));
  
  // Handle all multi-byte conversions into the host byte order.
  dst->pl_mgic  = ntohl(src->pl_mgic);
  dst->pl_port  = ntohs(src->pl_port);
  dst->pl_len   = ntohs(src->pl_len);
  dst->pl_snum  = ntohll(src->pl_snum);
  dst->pl_slen  = ntohll(src->pl_slen);
  dst->pl_laddr = ntohll(src->pl_laddr);
  dst->pl_haddr = ntohll(src->pl_haddr);
  dst->pl_key   = ntohll(src->pl_key);
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
/// @global wrapper
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

  // Prepare payload data for transport. First step is to encode the payload
  // to ensure correct handling of endianness of the multi-byte integers.
  // Second step consists of placing the encoded payload at start of the
  // wrapper buffer to allow for artificially extending the payload.
  encode_payload(&npl, hpl);
  (void)memcpy(wrapper, &npl, sizeof(npl));
  (void)memset(&iov, 0, sizeof(iov));
  iov.iov_base = wrapper;
  iov.iov_len  = hpl->pl_len;

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
  if (len == -1 || len != (ssize_t)hpl->pl_len) {
    log(lvl, true, "unable to send a payload");
    pr->pr_stat.st_seni++;
    return false;
  }

  return true;
}

/// Receive datagrams on both IPv4 and IPv6.
/// @return success/failure indication
///
/// @global wrapper
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
  bool ctl;

  log(LL_TRACE, false, "receiving a packet");

  // Prepare payload data.
  (void)memset(&iov, 0, sizeof(iov));
  iov.iov_base = wrapper;
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

  // Ensure that at least the base payload has arrived.
  if (len < (ssize_t)sizeof(*npl)) {
    log(lvl, false, "insufficient payload length");
    pr->pr_stat.st_resz++;
    return false;
  }

	// Check for received packet payload size.
	if (len < (ssize_t)sizeof(*npl) || msg.msg_flags & MSG_TRUNC) {
	  log(lvl, false, "wrong payload size, expected %zd, actual %zu", len, sizeof(*npl));
    pr->pr_stat.st_resz++;
    return false;
	}

	// Check for received packet control payload size. Inability to receive the
  // control data is not considered to be critical to the main purpose of the
  // application.
  ctl = true;
	if (msg.msg_flags & MSG_CTRUNC) {
	  log(LL_DEBUG, false, "control data was truncated");
    ctl = false;
	}

  // Unpack the payload from the wrapper and convert the payload from its
  // on-wire format.
  (void)memcpy(&npl, wrapper, sizeof(npl));
  decode_payload(hpl, npl);

  // Now that the base part of the payload is decoded, we can examine whether
  // the actual length of the datagram matches the expected length.
  if (len != (ssize_t)hpl->pl_len) {
	  log(lvl, false, "wrong payload size, expected %zd, actual %" PRIu64, len, hpl->pl_len);
  }

	// Obtain the TTL/hops value, if the control data was successfully received.
  // If not, an invalid TTL/hops value of 0 is used.
  if (ctl == true) {
    retrieve_ttl(ttl, &msg);
  } else {
    *ttl = 0;   
  }

  // Verify the payload correctness.
  retb = verify_payload(&pr->pr_stat, hpl);
  if (retb == false) {
    log(LL_WARN, false, "invalid payload");
    return false;
  }

  return true;
}
