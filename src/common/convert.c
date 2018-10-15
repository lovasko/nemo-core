// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <arpa/inet.h>

#include "common/convert.h"


/// Convert time in only nanoseconds into seconds and nanoseconds.
///
/// @param[out] ts seconds and nanoseconds
/// @param[in]  ns nanoseconds
void
fnanos(struct timespec* ts, const uint64_t ns)
{
  ts->tv_sec  = (time_t)(ns / 1000000000ULL);
  ts->tv_nsec = ns % 1000000000ULL;
}

/// Convert time in second and nanoseconds into only nanoseconds.
///
/// @param[out] ns nanoseconds
/// @param[in]  ts seconds and nanoseconds
void
tnanos(uint64_t* ns, const struct timespec ts)
{
  *ns = (uint64_t)ts.tv_nsec + ((uint64_t)ts.tv_sec * 1000000000ULL);
}

/// Encode a 64-bit unsigned integer for a reliable network transmission.
/// @return encoded integer
///
/// @param[in] x integer
uint64_t
htonll(const uint64_t x)
{
  uint32_t hi;
  uint32_t lo;

  hi = x >> 32;
  lo = x & 0xffffffff;

  return (uint64_t)htonl(lo) | ((uint64_t)htonl(hi) << 32);
}

/// Decode a 64-bit unsigned integer that was transmitted over a network.
/// @return decoded integer
///
/// @param[in] x integer
uint64_t
ntohll(const uint64_t x)
{
  uint32_t hi;
  uint32_t lo;

  hi = x >> 32;
  lo = x & 0xffffffff;

  return (uint64_t)ntohl(lo) | ((uint64_t)ntohl(hi) << 32);
}

/// Convert the standard IPv6 address structure into two 64-bit unsigned
/// integers.
///
/// @param[out] lo   low-bits of the address
/// @param[out] hi   high-bits of the address
/// @param[in]  addr IPv6 address structure
void
fipv6(uint64_t* lo, uint64_t* hi, const struct in6_addr addr)
{
  *lo = (uint64_t)addr.s6_addr[0]
      | (uint64_t)addr.s6_addr[1]  << 8
      | (uint64_t)addr.s6_addr[2]  << 16
      | (uint64_t)addr.s6_addr[3]  << 24
      | (uint64_t)addr.s6_addr[4]  << 32
      | (uint64_t)addr.s6_addr[5]  << 40
      | (uint64_t)addr.s6_addr[6]  << 48
      | (uint64_t)addr.s6_addr[7]  << 56;

  *hi = (uint64_t)addr.s6_addr[8]
      | (uint64_t)addr.s6_addr[9]  << 8
      | (uint64_t)addr.s6_addr[10] << 16
      | (uint64_t)addr.s6_addr[11] << 24
      | (uint64_t)addr.s6_addr[12] << 32
      | (uint64_t)addr.s6_addr[13] << 40
      | (uint64_t)addr.s6_addr[14] << 48
      | (uint64_t)addr.s6_addr[15] << 56;
}

/// Convert two 64-bit unsigned integers into the standard IPv6 address
/// structure.
///
/// @param[out] addr IPv6 address structure
/// @param[in]  lo   low-bits of the address
/// @param[in]  hi   high-bits of the address
void
tipv6(struct in6_addr* addr, const uint64_t lo, const uint64_t hi)
{
  addr->s6_addr[0]  = lo & 0x00000000000000ffULL;
  addr->s6_addr[1]  = lo & 0x000000000000ff00ULL;
  addr->s6_addr[2]  = lo & 0x0000000000ff0000ULL;
  addr->s6_addr[3]  = lo & 0x00000000ff000000ULL;
  addr->s6_addr[4]  = lo & 0x000000ff00000000ULL;
  addr->s6_addr[5]  = lo & 0x0000ff0000000000ULL;
  addr->s6_addr[6]  = lo & 0x00ff000000000000ULL;
  addr->s6_addr[7]  = lo & 0xff00000000000000ULL;

  addr->s6_addr[8]  = hi & 0x00000000000000ffULL;
  addr->s6_addr[9]  = hi & 0x000000000000ff00ULL;
  addr->s6_addr[10] = hi & 0x0000000000ff0000ULL;
  addr->s6_addr[11] = hi & 0x00000000ff000000ULL;
  addr->s6_addr[12] = hi & 0x000000ff00000000ULL;
  addr->s6_addr[13] = hi & 0x0000ff0000000000ULL;
  addr->s6_addr[14] = hi & 0x00ff000000000000ULL;
  addr->s6_addr[15] = hi & 0xff00000000000000ULL;
}

/// Encode the payload to the on-wire format. This function only deals with the
/// multi-byte fields, as all single-byte fields are correctly interpreted on
/// all endian encodings.
///
/// @param[out] pl payload
void
encode_payload(struct payload* pl)
{
  pl->pl_mgic  = htonl(pl->pl_mgic);
  pl->pl_port  = htons(pl->pl_port);
  pl->pl_snum  = htonll(pl->pl_snum);
  pl->pl_slen  = htonll(pl->pl_slen);
  pl->pl_laddr = htonll(pl->pl_laddr);
  pl->pl_haddr = htonll(pl->pl_haddr);
  pl->pl_reqk  = htonll(pl->pl_reqk);
  pl->pl_resk  = htonll(pl->pl_resk);
  pl->pl_mtm1  = htonll(pl->pl_mtm1);
  pl->pl_rtm1  = htonll(pl->pl_rtm1);
  pl->pl_mtm2  = htonll(pl->pl_mtm2);
  pl->pl_rtm2  = htonll(pl->pl_rtm2);
}

/// Decode the on-wire format of the payload. This function only deals with the
/// multi-byte fields, as all single-byte fields are correctly interpreted on
/// all endian encodings.
///
/// @param[out] pl payload
void
decode_payload(struct payload* pl)
{
  pl->pl_mgic  = ntohl(pl->pl_mgic);
  pl->pl_port  = ntohs(pl->pl_port);
  pl->pl_snum  = ntohll(pl->pl_snum);
  pl->pl_slen  = ntohll(pl->pl_slen);
  pl->pl_laddr = ntohll(pl->pl_laddr);
  pl->pl_haddr = ntohll(pl->pl_haddr);
  pl->pl_reqk  = ntohll(pl->pl_reqk);
  pl->pl_resk  = ntohll(pl->pl_resk);
  pl->pl_mtm1  = ntohll(pl->pl_mtm1);
  pl->pl_rtm1  = ntohll(pl->pl_rtm1);
  pl->pl_mtm2  = ntohll(pl->pl_mtm2);
  pl->pl_rtm2  = ntohll(pl->pl_rtm2);
}
