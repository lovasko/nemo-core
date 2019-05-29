// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <arpa/inet.h>
#include <netinet/in.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "common/log.h"
#include "common/convert.h"
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
  (void)printf("key,seq_num,seq_len,addr_res,"
               "ttl_dep_req,ttl_arr_res,ttl_dep_res,ttl_arr_req,"
               "real_dep_req,real_arr_res,real_arr_req,"
               "mono_dep_req,mono_arr_res,mono_arr_req\n");
}

/// Report the event of the incoming datagram by printing a CSV-formatted line
/// to the standard output stream.
///
/// @param[in] hpl  payload in host byte order
/// @param[in] npl  payload in network byte order
/// @param[in] port requesters port
/// @param[in] cf   configuration
void
report_event(const struct payload* hpl,
             const struct payload* npl,
             const uint64_t mono,
             const uint64_t real,
             const uint8_t ttl,
             const struct config* cf)
{
  char addrstr[INET6_ADDRSTRLEN];
  char ttl2str[4];
  char ttl4str[4];
  struct in_addr a4;
  struct in6_addr a6;

  // No output to be performed if the silent mode was requested.
  if (cf->cf_sil == true) {
    return;
  }

  // Binary mode expects the payload in a on-wire encoding.
  if (cf->cf_bin == true) {
    (void)fwrite(npl, sizeof(*npl), 1, stdout);
    return;
  }

  (void)memset(addrstr, '\0', sizeof(addrstr));
  (void)memset(ttl2str, '\0', sizeof(ttl2str));
  (void)memset(ttl4str, '\0', sizeof(ttl4str));

  // Convert the IP address into a string.
  if (cf->cf_ipv4 == true) {
    a4.s_addr = (uint32_t)hpl->pl_laddr;
    (void)inet_ntop(AF_INET, &a4, addrstr, sizeof(addrstr));
  } else {
    tipv6(&a6, hpl->pl_laddr, hpl->pl_haddr);
    (void)inet_ntop(AF_INET6, &a6, addrstr, sizeof(addrstr));
  }

  // If no TTL was received, report it as not available.
  if (hpl->pl_ttl2 == 0) {
    (void)strncpy(ttl2str, "N/A", sizeof(ttl2str));
  } else {
    (void)snprintf(ttl2str, sizeof(ttl2str), "%" PRIu8, hpl->pl_ttl2);
  }
  // If no TTL was received, report it as not available.
  if (ttl == 0) {
    (void)strncpy(ttl4str, "N/A", sizeof(ttl4str));
  } else {
    (void)snprintf(ttl4str, sizeof(ttl4str), "%" PRIu8, ttl);
  }

  (void)printf("%" PRIu64 ","   // key
               "%" PRIu64 ","   // seq_num
               "%" PRIu64 ","   // seq_len
               "%s,"            // addr_res
               "%" PRIu8  ","   // ttl_dep_req
               "%s,"            // ttl_arr_res
               "%" PRIu8  ","   // ttl_dep_res
               "%s,"            // ttl_arr_req
               "%" PRIu64 ","   // real_dep_req
               "%" PRIu64 ","   // real_arr_res
               "%" PRIu64 ","   // real_arr_req
               "%" PRIu64 ","   // mono_dep_req
               "%" PRIu64 ","   // mono_arr_res
               "%" PRIu64 "\n", // mono_arr_req
               hpl->pl_key,  hpl->pl_snum, hpl->pl_slen, addrstr,
               hpl->pl_ttl1, ttl2str, hpl->pl_ttl3, ttl4str,
               hpl->pl_rtm1, hpl->pl_rtm2, real,
               hpl->pl_mtm1, hpl->pl_mtm2, mono);
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
