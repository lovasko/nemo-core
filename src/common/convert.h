// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_CONVERT_H
#define NEMO_COMMON_CONVERT_H

#include <netinet/in.h>

#include <stdint.h>
#include <time.h>

#include "common/payload.h"


// Nanosecond conversion.
void fnanos(struct timespec* ts, const uint64_t ns);
void tnanos(uint64_t* ns, const struct timespec ts);

// 64-bit unsigned integer endian conversion.
uint64_t htonll(const uint64_t x);
uint64_t ntohll(const uint64_t x);

// IPv6 address conversion.
void fipv6(uint64_t* lo, uint64_t* hi, const struct in6_addr addr);
void tipv6(struct in6_addr* addr, const uint64_t lo, const uint64_t hi);

#endif
