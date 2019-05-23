// Copyright (c) 2018-2019 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/socket.h>

#include <string.h>

#include "common/packet.h"
#include "common/log.h"
#include "common/now.h"
#include "common/convert.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


/// Fill the payload with default and selected data.
///
/// @param[in] hpl  payload in host byte order
/// @param[in] tg   network target
/// @param[in] snum sequence number
/// @param[in] cf   configuration
static void
fill_payload(struct payload *hpl,
             const struct target* tg,
             const uint64_t snum,
             const struct config* cf)
{
  (void)memset(hpl, 0, sizeof(*hpl));
  hpl->pl_mgic  = NEMO_PAYLOAD_MAGIC;
  hpl->pl_fver  = NEMO_PAYLOAD_VERSION;
  hpl->pl_type  = NEMO_PAYLOAD_TYPE_REQUEST;
  hpl->pl_port  = (uint16_t)cf->cf_port;
  hpl->pl_ttl1  = (uint8_t)cf->cf_ttl;
  hpl->pl_len   = (uint16_t)cf->cf_len;
  hpl->pl_snum  = snum;
  hpl->pl_slen  = cf->cf_cnt;
  hpl->pl_key   = cf->cf_key;
  hpl->pl_laddr = tg->tg_laddr;
  hpl->pl_haddr = tg->tg_haddr;
  hpl->pl_rtm1  = real_now();
  hpl->pl_mtm1  = mono_now();
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

  if (cf->cf_ipv4 == true) {
    (void)memset(&sin, 0, sizeof(sin));
    sin.sin_family      = AF_INET;
    sin.sin_port        = htons((uint16_t)cf->cf_port);
    sin.sin_addr.s_addr = (uint32_t)tg->tg_laddr;

    (void)memcpy(ss, &sin, sizeof(sin));
  } else {
    (void)memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port   = htons((uint16_t)cf->cf_port);
    tipv6(&sin6.sin6_addr, tg->tg_laddr, tg->tg_haddr);

    (void)memcpy(ss, &sin6, sizeof(sin6));
  }
}

/// Issue a request against a target.
/// @return success/failure indication
///
/// @param[out] pr   protocol
/// @param[in]  snum sequence number
/// @param[in]  tg   network target
/// @param[in]  cf   configuration
static bool
issue_request(struct proto* pr,
              const uint64_t snum,
              const struct target* tg,
              const struct config* cf)
{
  bool retb;
  struct payload hpl;
  struct sockaddr_storage addr;

  // Prepare data for transmission.
  fill_payload(&hpl, tg, snum, cf);
  set_address(&addr, tg, cf);

  // Handle the IPv4 case.
  retb = send_packet(pr, &hpl, addr, cf->cf_err);
  if (retb == false) {
    log(LL_WARN, false, "unable to send a request");
    return false;
  }

  return true;
}

/// Single round of issued requests with small pauses after each request.
/// @return success/failure indication
///
/// @param[out] pr  protoco
/// @param[in]  tg  array of network targets
/// @param[in]  ntg number of network targets
/// @param[in]  sn  sequence number
/// @param[in]  cf  configuration
bool
dispersed_round(struct proto* pr,
                const struct target* tg,
                const uint64_t ntg,
                const uint64_t snum,
                const struct config* cf)
{
  uint64_t i;
  uint64_t part;
  bool retb;

  // In case there are no targets, just sleep throughout the whole round.
  if (ntg == 0) {
    retb = wait_for_events(pr, cf->cf_int, cf);
    if (retb == false) {
      log(LL_WARN, false, "unable to wait for events");
      return false;
    }

    return true;
  }

  // Compute the time to sleep between each request in the round. We can safely
  // divide by the number of targets, as we have previously handled the case of
  // no targets.
  part = (cf->cf_int / ntg) + 1;

  // Issue all requests.
  for (i = 0; i < ntg; i++) {
    retb = issue_request(pr, snum, &tg[i], cf);
    if (retb == false) {
      return false;
    }

    // Await events for the appropriate fraction of the round.
    retb = wait_for_events(pr, part, cf);
    if (retb == false) {
      log(LL_WARN, false, "unable to wait for events");
      return false;
    }
  }

  return true;
}

/// Single round of issued requests with no pauses after each request, followed
/// by a single full pause.
/// @return success/failure indication
///
/// @param[out] pr  protocol
/// @param[in]  tg  array of network targets
/// @param[in]  ntg number of network targets
/// @param[in]  sn  sequence number
/// @param[in]  cf  configuration
bool
grouped_round(struct proto* pr,
              const struct target* tg,
              const uint64_t ntg,
              const uint64_t snum,
              const struct config* cf)
{
  uint64_t i;
  bool retb;

  // Issue all requests.
  for (i = 0; i < ntg; i++) {
    retb = issue_request(pr, snum, &tg[i], cf);
    if (retb == false) {
      return false;
    }
  }

  // Await events for the remainder of the interval.
  retb = wait_for_events(pr, cf->cf_int, cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to wait for events");
    return false;
  }

  return true;
}
