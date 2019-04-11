// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <inttypes.h>
#include <stdbool.h>

#include "common/stats.h"
#include "common/log.h"


/// Reset all protocol stats.
///
/// @param[in] st stats
void
reset_stats(struct stats* st)
{
  st->st_rall = 0;
  st->st_reni = 0;
  st->st_resz = 0;
  st->st_remg = 0;
  st->st_repv = 0;
  st->st_rety = 0;
  st->st_sall = 0;
  st->st_seni = 0;
}

/// Log all protocol stats.
///
/// @param[in] pn protocol name 
/// @param[in] st stats
void 
log_stats(const char* pn, const struct stats* st)
{
  log(LL_DEBUG, false, "statistics for %s:", pn);
  log(LL_DEBUG, false, "overall received: %" PRIu64, st->st_rall);
  log(LL_DEBUG, false, "receive network-related errors: %" PRIu64, st->st_reni);
  log(LL_DEBUG, false, "receive datagram size mismatches: %" PRIu64, st->st_resz);
  log(LL_DEBUG, false, "receive payload magic mismatches: %" PRIu64, st->st_remg);
  log(LL_DEBUG, false, "receive payload version mismatches: %" PRIu64, st->st_repv);
  log(LL_DEBUG, false, "receive payload type mismatches: %" PRIu64, st->st_rety);
  log(LL_DEBUG, false, "overall sent: %" PRIu64, st->st_sall);
  log(LL_DEBUG, false, "send network-related errors: %" PRIu64, st->st_seni);
}
