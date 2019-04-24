// Copyright (c) 2018-2019 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include "common/log.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


/// Issue a request against a target.
/// @return success/failure indication
///
/// @param[out] p4   IPv4 protocol connection
/// @param[out] p6   IPv6 protocol connection
/// @param[in]  snum sequence number
/// @param[in]  tg   network target
/// @param[in]  cf   configuration
static bool
contact_target(struct proto* p4,
               struct proto* p6,
               const uint64_t snum,
               const struct target* tg,
               const struct config* cf)
{
  bool retb;

  // Handle the IPv4 case.
  if (cf->cf_ipv4 == true && tg->tg_ipv == NEMO_IP_VERSION_4) { 
    retb = send_request(p4, snum, tg, cf);
    if (retb == false) {
      log(LL_WARN, false, "unable to send a request");
      return false;
    }
  }

  // Handle the IPv6 case.
  if (cf->cf_ipv6 == true && tg->tg_ipv == NEMO_IP_VERSION_6) { 
    retb = send_request(p6, snum, tg, cf);
    if (retb == false) {
      log(LL_WARN, false, "unable to send a request");
      return false;
    }
  }

  return true;
}

/// Single round of issued requests with small pauses after each request.
/// @return success/failure indication
///
/// @param[out] p4  IPv4 protocol connection
/// @param[out] p6  IPv6 protocol connection
/// @param[in]  tg  array of network targets
/// @param[in]  ntg number of network targets
/// @param[in]  sn  sequence number
/// @param[in]  cf  configuration
bool
dispersed_round(struct proto* p4,
                struct proto* p6,
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
    retb = wait_for_events(p4, p6, cf->cf_int, cf);
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
    retb = contact_target(p4, p6, snum, &tg[i], cf);
    if (retb == false) {
      return false;
    }

    // Await events for the appropriate fraction of the round.
    retb = wait_for_events(p4, p6, part, cf);
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
/// @param[out] p4  IPv4 protocol connection
/// @param[out] p6  IPv6 protocol connection
/// @param[in]  tg  array of network targets
/// @param[in]  ntg number of network targets
/// @param[in]  sn  sequence number
/// @param[in]  cf  configuration
bool
grouped_round(struct proto* p4,
              struct proto* p6,
              const struct target* tg,
              const uint64_t ntg,
              const uint64_t snum,
              const struct config* cf)
{
  uint64_t i;
  bool retb;

  // Issue all requests.
  for (i = 0; i < ntg; i++) {
    retb = contact_target(p4, p6, snum, &tg[i], cf);
    if (retb == false) {
      return false;
    }
  }

  // Await events for the remainder of the interval.
  retb = wait_for_events(p4, p6, cf->cf_int, cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to wait for events");
    return false;
  }

  return true;
}
