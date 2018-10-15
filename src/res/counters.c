// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <inttypes.h>

#include "common/log.h"
#include "res/proto.h"
#include "res/types.h"


/// Reset all counters to zero.
///
/// @param[out] cts counters
void
reset_counters(struct counters* cts)
{
  cts->ct_rall = 0;
  cts->ct_reni = 0;
  cts->ct_resz = 0;
  cts->ct_remg = 0;
  cts->ct_repv = 0;
  cts->ct_rety = 0;
  cts->ct_sall = 0;
  cts->ct_seni = 0;
}

/// Log all counters using the information level.
///
/// @param[in] ipv IP protocol version name
/// @param[in] cts counters
void 
log_counters(const char* ipv, const struct counters* cts)
{
  log(LL_DEBUG, false, "main", "%s overall received: %" PRIu64, ipv,
    cts->ct_rall);
  log(LL_DEBUG, false, "main", "%s receive network-related errors: %" PRIu64,
    ipv, cts->ct_reni);
  log(LL_DEBUG, false, "main", "%s receive datagram size mismatches: %" PRIu64,
    ipv, cts->ct_resz);
  log(LL_DEBUG, false, "main", "%s receive payload magic mismatches: %"
    PRIu64, ipv, cts->ct_remg);
  log(LL_DEBUG, false, "main", "%s receive payload version mismatches: %"
    PRIu64, ipv, cts->ct_repv);
  log(LL_DEBUG, false, "main", "%s receive payload type mismatches: %" PRIu64,
    ipv, cts->ct_rety);
  log(LL_DEBUG, false, "main", "%s overall sent: %" PRIu64, ipv, cts->ct_sall);
  log(LL_DEBUG, false, "main", "%s send network-related errors: %" PRIu64, ipv,
    cts->ct_seni);
}
