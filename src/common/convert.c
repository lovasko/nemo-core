// Copyright (c) 2018-2019 Daniel Lovasko
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
  ts->tv_nsec = (long)(ns % 1000000000ULL);
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

  hi = (uint32_t)(x >> 32);
  lo = (uint32_t)(x & 0xffffffff);

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

  hi = (uint32_t)(x >> 32);
  lo = (uint32_t)(x & 0xffffffff);

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
  uint8_t i;

  *lo = 0;
  for (i = 0; i < 8; i++)
    *lo |= (uint64_t)addr.s6_addr[i]     << (i * 8);

  *hi = 0;
  for (i = 0; i < 8; i++)
    *hi |= (uint64_t)addr.s6_addr[i + 8] << (i * 8);
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
  uint8_t i;

  for (i = 0; i < 8; i++)
    addr->s6_addr[i]     = (uint8_t)(lo >> (8 * i)) & 0xff;

  for (i = 0; i < 8; i++)
    addr->s6_addr[i + 8] = (uint8_t)(hi >> (8 * i)) & 0xff;
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
