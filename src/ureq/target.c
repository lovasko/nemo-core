// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netdb.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "ureq/funcs.h"
#include "ureq/types.h"
#include "common/log.h"


/// Create a part of a IPv6 address by converting array of bytes into a
/// single integer.
/// @return portion of IPv6 address
///
/// @param[in] ab address bytes 
static uint64_t
ipv6_part(const uint8_t* ab)
{
  uint64_t res;
  uint8_t i;

  res = 0;
  for (i = 0; i < 8; i++)
    res |= ((uint64_t)ab[i] << (i * 8));

  return res;
}

/// Convert a IPv4 address into a network target.
///
/// @param[out] tg network target
/// @param[in]  a4 IPv4 address
static void
read_target4(struct target* tg, const struct in_addr* a4)
{
  tg->tg_ipv   = NEMO_IP_VERSION_4;
  tg->tg_laddr = (uint64_t)a4->s_addr;
  tg->tg_haddr = (uint64_t)0;
}

/// Convert a IPv6 address into a network target.
///
/// @param[out] tg network target
/// @param[in]  a6 IPv6 address
static void
read_target6(struct target* tg, const struct in6_addr* a6)
{
  tg->tg_ipv   = NEMO_IP_VERSION_6;
  tg->tg_laddr = ipv6_part(&a6->s6_addr[0]);
  tg->tg_haddr = ipv6_part(&a6->s6_addr[7]);
}

/// Resolve a domain name into multiple network targets.
/// @return success/failure indication
///
/// @param[out] tg   array of targets
/// @param[out] tcnt number of targets
/// @param[in]  tmax max number of targets
/// @param[in]  name name to resolve
/// @param[in]  cf   configuration
static bool
resolve_name(struct target* tg,
             uint64_t* tcnt,
             const uint64_t tmax,
             const char* name,
             const struct config* cf)
{
  int reti;
  struct addrinfo hint;
  struct addrinfo* ais;
  struct addrinfo* ai;
  char estr[128];
  uint8_t lvl;

  // Prepare the logging level for network-related errors.
  lvl = cf->cf_err == true ? LL_WARN : LL_DEBUG;

  // Prepare the look-up settings.
  (void)memset(&hint, 0, sizeof(hint));
  hint.ai_flags    = 0;
  hint.ai_socktype = SOCK_DGRAM;
  hint.ai_family   = AF_UNSPEC;
  hint.ai_protocol = 0;

  // Resolve the domain name to a list of address information.
  reti = getaddrinfo(name, NULL, &hint, &ais);
  if (reti != 0) {
    // Due to the getaddrinfo(3) function not using errno, we have to replicate
    // the expected error format this way.
    (void)snprintf(estr, sizeof(estr), "unable to resolve name '%%s': %s",
      gai_strerror(reti));
    log(lvl, false, "main", estr, name);

    if (cf->cf_err == true)
      return false;
  }

  // Traverse the returned address information.
  *tcnt = 0;
  for (ai = ais; ai != NULL; ai = ai->ai_next) {
    // Verify that we are not exceeding the maximal number of targets.
    if (*tcnt == tmax) {
      log(lvl, false, "main", "reached maximum number of targets per name: %"
        PRIu64, tmax);

      if (cf->cf_err == true)
        return false;
      else
        break;
    }

    // Convert the address to a IPv4 target.
    if (cf->cf_ipv4 == true && ai->ai_family == AF_INET) {
      read_target4(&tg[*tcnt], &((struct sockaddr_in*)ai->ai_addr)->sin_addr);
      (*tcnt)++;
    }

    // Convert the address to a IPv6 target.
    if (cf->cf_ipv6 == true && ai->ai_family == AF_INET6) {
      read_target6(&tg[*tcnt], &((struct sockaddr_in6*)ai->ai_addr)->sin6_addr);
      (*tcnt)++;
    }
  }

  freeaddrinfo(ais);

  return true;
}

/// Parse a string into a network target.
/// @return success/failure indication 
///
/// @param[out] tg   array of targets
/// @param[out] tcnt number of targets
/// @param[in]  tmax maximal number of targets
/// @param[in]  tstr target string
/// @param[in]  cf   configuration
static bool
parse_target_string(struct target* tg,
                    uint64_t* tcnt,
                    const uint64_t tmax,
                    const char* tstr,
                    const struct config* cf)
{
  int reti;
  bool retb;
  struct in_addr a4;
  struct in6_addr a6;

  // Prepare the target structure.
  (void)memset(tg, 0, sizeof(tg[0]) * tmax);

  // Try parsing the address as numeric IPv4 address.
  if (cf->cf_ipv4 == true) {
    reti = inet_pton(AF_INET, tstr, &a4);
    if (reti == 1) {
      read_target4(&tg[0], &a4);
      *tcnt = 0;

      log(LL_TRACE, false, "main", "parsed %s target: %s", "IPv4", tstr);
      return true;
    }
  }

  // Try parsing the address as numeric IPv6 address.
  if (cf->cf_ipv6 == true) {
    reti = inet_pton(AF_INET6, tstr, &a6);
    if (reti == 1) {
      read_target6(&tg[0], &a6);
      *tcnt = 0;

      log(LL_TRACE, false, "main", "parsed %s target: %s", "IPv6", tstr);
      return true;
    }
  }

  // As none of the two address families were applicable, we assume that the
  // string is a domain name.
  if (reti == -1) {
    retb = resolve_name(tg, tcnt, tmax, tstr, cf);
    if (retb == false) {
      log(LL_TRACE, false, "main", "unable to parse target '%s'", tstr);
      return false;
    }
  }

  return true;
}

/// Compare two targets for equality.
/// @return comparison enum
/// @retval -1 tg1 >  tg2
/// @retval  0 tg1 == tg2
/// @retval  1 tg1 <  tg2
///
/// @param[in] tg1 first target
/// @param[in] tg2 second target
static int
compare_targets(const void* tg1, const void* tg2)
{
  return memcmp(tg1, tg2, sizeof(struct target));
}

/// Normalize the target array by removing duplicates and sorting the addresses
/// in the ascending order.
///
/// @param[out] tg   array of targets
/// @param[out] nlen new length of the array
/// @param[in]  olen old length of the array
static void
normalize_targets(struct target* tg, uint64_t* nlen, const uint64_t olen)
{
  uint64_t idx;
  uint64_t lst;

  // Early exit for empty or a trivial array.
  if (olen == 0 || olen == 1)
    return;

  // Pre-sort the array of targets. This operation is expected to run in
  // O(n * log n) time.
  qsort(tg, olen, sizeof(tg[0]), compare_targets);

  // Traverse the array once while removing duplicate elements. It is possible
  // to execute this in O(n) time due to the sorting of the array beforehand.
  lst = 0;
  for (idx = 1; idx < olen; idx++) {
    if (compare_targets(&tg[idx], &tg[lst]) != 0) {
      lst++;
      (void)memcpy(&tg[lst], &tg[idx], sizeof(tg[0]));
    }
  }

  *nlen = lst + 1;
}

/// Parse all network targets and convert them into binary addresses.
/// @return success/failure indication
///
/// @param[out] tg array of targets
/// @param[out] nl new length of the array
/// @param[in]  cf configuration
bool
load_targets(struct target* tg,
             uint64_t* tcnt,
             const uint64_t tmax,
             const struct config* cf)
{
  uint64_t idx;
  bool retb;
  uint8_t lvl;
  struct target tg2[32];
  uint64_t tcnt1;
  uint64_t tcnt2;
  uint64_t tall;

  // Prepare the logging level for network-related errors.
  lvl = cf->cf_err == true ? LL_WARN : LL_DEBUG;

  // Traverse all targets listed in the configuration.
  tall = 0;
  for (idx = 0; cf->cf_tg[idx] != NULL; idx++) {
    // Obtain a set of targets based on the target string.
    retb = parse_target_string(tg2, &tcnt1, 32, cf->cf_tg[idx], cf);
    if (retb == false)
      return false;

    // Remove duplicates and sort the targets.
    normalize_targets(tg2, &tcnt2, tcnt1);

    // Verify that we are not reaching the overall limit on targets.
    if (tcnt2 + tall - 1 > tmax) {
      log(lvl, false, "main", "unable to append more targets");

      if (cf->cf_err == true)
        return false;
      else
        break;
    }

    // Append the obtained targets to the overall list.
    (void)memcpy(tg + tall, tg2, tcnt2 * sizeof(tg2[0]));
    tall += tcnt2;
  }
 
  // Final normalization sweep.
  normalize_targets(tg, tcnt, tcnt2);

  return true;
}