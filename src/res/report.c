// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <arpa/inet.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "res/proto.h"
#include "res/types.h"
#include "util/convert.h"
#include "util/log.h"


/// Print the CSV header of the reporting output.
///
/// @param[in] cf configuration
void
report_header(const struct config* cf)
{
  // No output to be performed if the silent mode was requested.
  if (cf->cf_sil == true)
    return;

  // Binary mode has no header.
  if (cf->cf_bin == true)
    return;

  // Print the CSV header of the standard output.
  (void)printf("ReqKey,ResKey,SeqNum,SeqLen,IPVer,Addr,Port,DepTTL,ArrTTL,"
               "DepRealTime,DepMonoTime,ArrRealTime,ArrMonoTime\n");
}

/// Report the event of the incoming datagram by printing a CSV-formatted line
/// to the standard output stream.
///
/// @param[in] pl payload
/// @param[in] cf configuration
void
report_event(const struct payload* pl, const struct config* cf)
{
  char addrstr[128];
  struct in_addr a4;
  struct in6_addr a6;
  char ttlstr[8];
  struct payload plout;

  // No output to be performed if the silent mode was requested.
  if (cf->cf_sil == true)
    return;

  // Binary mode expects the payload in a on-wire encoding.
  if (cf->cf_bin == true) {
    (void)memcpy(&plout, pl, sizeof(plout));
    encode_payload(&plout);
    (void)fwrite(&plout, sizeof(plout), 1, stdout);

    return;
  }

  (void)memset(addrstr, '\0', sizeof(addrstr));
  (void)memset(ttlstr,  '\0', sizeof(ttlstr));

  // Convert the IP address into a string.
  if (pl->pl_pver == 4) {
    a4.s_addr = (uint32_t)pl->pl_laddr;
    inet_ntop(AF_INET, &a4, addrstr, sizeof(addrstr));
  } else {
    tipv6(&a6, pl->pl_laddr, pl->pl_haddr);
    inet_ntop(AF_INET6, &a6, addrstr, sizeof(addrstr));
  }

  // If no TTL was received, report is as not available.
  if (pl->pl_ttl2 == 0)
    (void)strncpy(ttlstr, "N/A", 3);
  else
    (void)snprintf(ttlstr, 4, "%" PRIu8, pl->pl_ttl2);

  (void)printf("%" PRIu64 ","   // ReqKey
               "%" PRIu64 ","   // ResKey
               "%" PRIu64 ","   // SeqNum
               "%" PRIu64 ","   // SeqLen
               "%" PRIu8  ","   // IPVer
               "%s,"            // Addr
               "%" PRIu16 ","   // Port
               "%" PRIu8  ","   // DepTTL
               "%s,"            // ArrTTL
               "%" PRIu64 ","   // DepRealTime
               "%" PRIu64 ","   // DepMonoTime
               "%" PRIu64 ","   // ArrRealTime
               "%" PRIu64 "\n", // ArrMonoTime
               pl->pl_reqk, pl->pl_resk,
               pl->pl_snum, pl->pl_slen,
               pl->pl_pver, addrstr, pl->pl_port,
               pl->pl_ttl1, ttlstr,
               pl->pl_rtm1, pl->pl_mtm1,
               pl->pl_rtm2, pl->pl_mtm2);
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
  if (cf->cf_sil == true)
    return true;

  log(LL_INFO, false, "main", "flushing standard output stream");

  // Flush all stdio buffers.
  reti = fflush(stdout);
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to flush the standard output");
    return false;
  }

  return true;
}
