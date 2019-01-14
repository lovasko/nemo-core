// Copyright (c) 2018-2019 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/select.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "common/log.h"
#include "common/convert.h"
#include "common/now.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


/// Fill the payload with default and selected data.
///
/// @param[out] pl   payload
/// @param[in]  tg   network target
/// @param[in]  snum sequence number
/// @param[in]  cf   configuration
static void
fill_payload(struct payload *pl,
             const struct target* tg,
             const uint64_t snum,
             const struct proto* pr,
             const struct config* cf)
{
  (void)memset(pl, 0, sizeof(*pl));
  pl->pl_mgic  = NEMO_PAYLOAD_MAGIC;
  pl->pl_fver  = NEMO_PAYLOAD_VERSION;
  pl->pl_type  = NEMO_PAYLOAD_TYPE_REQUEST;
  pl->pl_port  = (uint16_t)cf->cf_port;
  pl->pl_ttl1  = (uint8_t)cf->cf_ttl;
  pl->pl_pver  = pr->pr_ipv;
  pl->pl_snum  = snum;
  pl->pl_slen  = cf->cf_cnt;
  pl->pl_reqk  = cf->cf_key;
  pl->pl_laddr = tg->tg_laddr;
  pl->pl_haddr = tg->tg_haddr;
  pl->pl_rtm1  = real_now();
  pl->pl_mtm1  = mono_now();
}

/// Convert the target address to a universal standard address type.
///
/// @param[out] ss universal address type
/// @param[in]  tg network target
static void
set_address(struct sockaddr_storage* ss,
            const struct target* tg,
            const struct config* cf)
{
  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;

  if (tg->tg_ipv == NEMO_IP_VERSION_4) {
    (void)memset(&sin, 0, sizeof(sin));
    sin.sin_family      = AF_INET;
    sin.sin_port        = htons((uint16_t)cf->cf_port);
    sin.sin_addr.s_addr = (uint32_t)tg->tg_laddr;
    (void)memcpy(ss, &sin, sizeof(sin));
  }

  if (tg->tg_ipv == NEMO_IP_VERSION_6) {
    (void)memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port   = htons((uint16_t)cf->cf_port);
    tipv6(&sin6.sin6_addr, tg->tg_laddr, tg->tg_haddr);
    (void)memcpy(ss, &sin6, sizeof(sin6));
  }
}

/// Send a request to a network target.
/// @return success/failure indication
///
/// @param[in] pr protocol connection
/// @param[in] tg network target
/// @param[in] cf configuration
bool
send_request(struct proto* pr,
             const uint64_t snum,
             const struct target* tg,
             const struct config* cf)
{
  struct payload pl;
  struct msghdr msg;
  struct iovec data;
  struct sockaddr_storage ss;
  ssize_t n;
  uint8_t lvl;

  log(LL_TRACE, false, "sending a request");

  // Prepare payload data.
  data.iov_base = &pl;
  data.iov_len  = sizeof(pl);

  // Prepare the message.
  set_address(&ss, tg, cf);
  msg.msg_name       = &ss;
  msg.msg_namelen    = sizeof(ss);
  msg.msg_iov        = &data;
  msg.msg_iovlen     = 1;
  msg.msg_control    = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags      = 0;

  // Prepare the payload content.
  fill_payload(&pl, tg, snum, pr, cf);
  encode_payload(&pl);

  // Send the datagram.
  n = sendmsg(pr->pr_sock, &msg, MSG_DONTWAIT);
  if (n == -1) {
    // Increase the seriousness of the incident in case we are going to fail.
    lvl = cf->cf_err == true ? LL_WARN : LL_DEBUG;
    log(lvl, true, "unable to send datagram");

    pr->pr_seni++;

    // Fail the procedure only if we have selected the exit-on-error attribute
    // to be true.
    return !cf->cf_err;
  }

  // Verify the number of sent bytes.
  if (n != (ssize_t)sizeof(pl)) {
    // Increase the seriousness of the incident in case we are going to fail.
    lvl = cf->cf_err == true ? LL_WARN : LL_DEBUG;
    log(lvl, true, "unable to send the whole payload");

    pr->pr_seni++;

    // Fail the procedure only if we have selected the exit-on-error attribute
    // to be true.
    return !cf->cf_err;
  }

  pr->pr_sall++;

  return true;
}

/// Receive a payload from the network.
/// @return success/failure indication
///
/// @param[out] pl payload
/// @param[out] pr protocol connection
/// @param[in]  cf configuration
bool
receive_response(struct payload* pl, struct proto* pr, const struct config* cf)
{
  struct msghdr msg;
  ssize_t n;
  bool retb;
  struct iovec data;
  struct sockaddr_storage addr;

  // Prepare payload data.
  data.iov_base = pl;
  data.iov_len  = sizeof(*pl);

  // Prepare the message.
  msg.msg_name    = &addr;
  msg.msg_namelen = sizeof(addr);
  msg.msg_iov     = &data;
  msg.msg_iovlen  = 1;
  msg.msg_flags   = 0;

  // Receive the message and handle potential errors.
  n = recvmsg(pr->pr_sock, &msg, MSG_DONTWAIT | MSG_TRUNC);
  if (n < 0) {
    log(LL_WARN, true, "receiving has failed");
    pr->pr_reni++;

    if (cf->cf_err == true)
      return false;
  }

  // Convert the payload from its on-wire format.
  decode_payload(pl);

  // Verify the payload correctness.
  retb = verify_payload(pr, n, pl);
  if (retb == false) {
    log(LL_WARN, false, "invalid payload content");
    return false;
  }

  return true;
}
