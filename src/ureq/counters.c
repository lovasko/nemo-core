// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <inttypes.h>

#include "common/log.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


/// Reset all protocol event counters to zero.
///
/// @param[out] pr protocol connection
void
reset_counters(struct proto* pr)
{
  pr->pr_rall = 0;
  pr->pr_reni = 0;
  pr->pr_resz = 0;
  pr->pr_remg = 0;
  pr->pr_repv = 0;
  pr->pr_rety = 0;
  pr->pr_sall = 0;
  pr->pr_seni = 0;
}

/// Log all counters using the information level.
///
/// @param[in] pr protocol connection
void 
log_counters(const struct proto* pr)
{
  log(LL_DEBUG, false, "%s overall received: %" PRIu64,
    pr->pr_name, pr->pr_rall);

  log(LL_DEBUG, false, "%s receive network-related errors: %" PRIu64,
    pr->pr_name, pr->pr_reni);

  log(LL_DEBUG, false, "%s receive datagram size mismatches: %" PRIu64,
    pr->pr_name, pr->pr_resz);

  log(LL_DEBUG, false, "%s receive payload magic mismatches: %" PRIu64,
    pr->pr_name, pr->pr_remg);

  log(LL_DEBUG, false, "%s receive payload version mismatches: %" PRIu64,
    pr->pr_name, pr->pr_repv);

  log(LL_DEBUG, false, "%s receive payload type mismatches: %" PRIu64,
    pr->pr_name, pr->pr_rety);

  log(LL_DEBUG, false, "%s overall sent: %" PRIu64,
    pr->pr_name, pr->pr_sall);

  log(LL_DEBUG, false, "%s send network-related errors: %" PRIu64,
    pr->pr_name, pr->pr_seni);
}
