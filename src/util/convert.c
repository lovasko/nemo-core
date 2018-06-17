// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <arpa/inet.h>

#include "convert.h"


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
