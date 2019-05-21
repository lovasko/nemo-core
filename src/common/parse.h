// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_PARSE_H
#define NEMO_COMMON_PARSE_H

#include <stdbool.h>
#include <stdint.h>


bool parse_uint64(uint64_t* out,
                  const char* str,
                  const uint64_t min,
                  const uint64_t max);

bool parse_scalar(uint64_t* out,
                  const char* inp,
                  const char* name,
                  const uint64_t min,
                  const uint64_t max,
                  bool (*func) (uint64_t*, const char*));

bool parse_memory_unit(uint64_t* mult, const char* unit);
bool parse_time_unit(uint64_t* mult, const char* unit);

#endif
