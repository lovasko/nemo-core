// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_UTIL_CONVERT_H
#define NEMO_UTIL_CONVERT_H

#include <stdint.h>
#include <time.h>


// Nanosecond conversion.
void fnanos(struct timespec* ts, const uint64_t ns);
void tnanos(uint64_t* ns, const struct timespec ts);

// 64-bit unsigned integer endian conversion.
uint64_t htonll(const uint64_t x);
uint64_t ntohll(const uint64_t x);

#endif
