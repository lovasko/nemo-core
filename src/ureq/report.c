// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdio.h>

#include "common/log.h"
#include "ureq/funcs.h"
#include "ureq/types.h"

/// Print the CSV header of the reporting output.
///
/// @param[in] cf configuration
void
report_header(const struct config* cf)
{
  // No output to be performed if the silent mode was requested.
  if (cf->cf_sil == true) {
    return;
  }

  // Binary mode has no header.
  if (cf->cf_bin == true) {
    return;
  }

  // Print the CSV header of the standard output.
  (void)printf("Type,ReqKey,ResKey,SeqNum,SeqLen,IPVer,Addr,Port,DepTTL,"
               "ArrTTL,DepRealTime,DepMonoTime,ArrRealTime,ArrMonoTime\n");
}

/// Flush all data written to the standard output to their respective device.
/// @return success/failure indication
///
/// @param[in] cf configuration
bool
flush_report_stream(const struct config* cf)
{
  int reti;

  // No output was performed if the silent mode was requested.
  if (cf->cf_sil == true) {
    return true;
  }

  log(LL_INFO, false, "flushing standard output stream");

  // Flush all stdio buffers.
  reti = fflush(stdout);
  if (reti == -1) {
    log(LL_WARN, true, "unable to flush the standard output");
    return false;
  }

  return true;
}
