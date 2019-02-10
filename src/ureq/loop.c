// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/select.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <inttypes.h>

#include "common/convert.h"
#include "common/log.h"
#include "common/now.h"
#include "common/payload.h"
#include "common/signal.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


/// Main request loop.
/// @return success/failure indication
///
/// @global shup
///
/// @param[in] p4 IPv4 protocol connection
/// @param[in] p6 IPv6 protocol connection
/// @param[in] cf configuration
bool
request_loop(struct proto* p4,
             struct proto* p6,
             struct target* tg,
             const struct config* cf)
{
  uint64_t i;
  uint64_t ntg;
  uint64_t rld;
  uint64_t now;
  bool retb;

  // Log the current configuration.
  log_config(cf);

  // Print the CSV header of the standard output.
  report_header(cf);

  // Load all targets at start.
  retb = load_targets(tg, &ntg, cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to load targets");
    return false;
  }

  // Set the next reload time to be in the future.
  now = mono_now();
  rld = now + cf->cf_rld;

  for (i = 0; i < cf->cf_cnt; i++) {
    log(LL_TRACE, false, "round %" PRIu64 "out of %" PRIu64, i + 1, cf->cf_cnt);

    // Check if name resolution needs to happen. This code contains a possible
    // race condition, in case a repeated SIGHUP signal appears between the
    // comparison and the clearing the `shup` flag. This is a conscious
    // decision, since the target re-loading is already in progress and will
    // therefore happen imminently, but only once (in case of two or more
    // SIGHUPs in immediate consequence).
    now = mono_now();
    if (now > rld || shup == true) {
      // Clear the SIGHUP flag.
      shup = false;

      // Re-load targets.
      retb = load_targets(tg, &ntg, cf);
      if (retb == false) {
        log(LL_WARN, false, "unable to re-load targets");
        return false;
      }

      // Update the next refresh time.
      rld = now + cf->cf_rld;
    }

    // Select the appropriate type of issuing requests in the round.
    if (cf->cf_grp == true) {
      retb = grouped_round(p4, p6, tg, ntg, i, cf);
      if (retb == false)
        return false;
    } else {
      retb = dispersed_round(p4, p6, tg, ntg, i, cf);
      if (retb == false)
        return false;
    }
  }

  // Await events after issuing all requests. The intention is to wait for
  // potential responses to the last few requests.
  log(LL_TRACE, false, "waiting for final events");
  retb = wait_for_events(p4, p6, cf->cf_wait, cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to wait for final events");
    return false;
  }

  return true;
}
