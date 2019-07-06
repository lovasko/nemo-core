// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <arpa/inet.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "common/convert.h"
#include "common/log.h"
#include "ures/funcs.h"
#include "ures/types.h"


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

  // Print the CSV header of the standard output.
  (void)printf("key,seq_num,seq_len,"
               "host_req,addr_req,port_req,host_res,"
               "ttl_dep_req,ttl_arr_res,"
               "real_dep_req,real_arr_res,"
               "mono_dep_req,mono_arr_res\n");
}

/// Report the event of the incoming datagram by printing a CSV-formatted line
/// to the standard output stream.
///
/// @param[in] pl payload
/// @param[in] hn local host name
/// @param[in] la low address bits of the requester
/// @param[in] ha high address bits of the requester
/// @param[in] pn UDP port of the requester
/// @param[in] cf configuration
void
report_event(const struct payload* pl,
             const char hn[static NEMO_HOST_NAME_SIZE],
             const uint64_t la,
             const uint64_t ha,
             const uint16_t pn,
             const struct config* cf)
{
  char addrstr[INET6_ADDRSTRLEN];
  struct in_addr a4;
  struct in6_addr a6;
  char ttlstr[8];

  // No output to be performed if the silent mode was requested.
  if (cf->cf_sil == true) {
    return;
  }

  (void)memset(addrstr, '\0', sizeof(addrstr));
  (void)memset(ttlstr,  '\0', sizeof(ttlstr));

  // Convert the IP address into a string.
  if (cf->cf_ipv4 == true) {
    a4.s_addr = (uint32_t)la;
    (void)inet_ntop(AF_INET, &a4, addrstr, sizeof(addrstr));
  } else {
    tipv6(&a6, la, ha);
    (void)inet_ntop(AF_INET6, &a6, addrstr, sizeof(addrstr));
  }

  // If no TTL was received, report it as not available.
  if (pl->pl_ttl2 == 0) {
    (void)strncpy(ttlstr, "N/A", sizeof(ttlstr));
  } else {
    (void)snprintf(ttlstr, sizeof(ttlstr), "%" PRIu8, pl->pl_ttl2);
  }

  (void)printf("%" PRIu64 ","   // key
               "%" PRIu64 ","   // seq_num
               "%" PRIu64 ","   // seq_len
               "%.*s,"          // host_req
               "%s,"            // addr_req
               "%" PRIu16 ","   // port_req
               "%.*s,"          // host_res
               "%" PRIu8  ","   // ttl_dep_req
               "%s,"            // ttl_arr_res
               "%" PRIu64 ","   // real_dep_req
               "%" PRIu64 ","   // real_arr_res
               "%" PRIu64 ","   // mono_dep_req
               "%" PRIu64 "\n", // mono_arr_res 
               pl->pl_key, pl->pl_snum, pl->pl_slen,
               NEMO_HOST_NAME_SIZE, pl->pl_host,
               addrstr, pn,
               NEMO_HOST_NAME_SIZE, hn, 
               pl->pl_ttl1, ttlstr,
               pl->pl_rtm1, pl->pl_rtm2,
               pl->pl_mtm1, pl->pl_mtm2);
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
