// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <inttypes.h>
#include <errno.h>

#include "parse.h"
#include "log.h"


/// Convert a string into an unsigned 64-bit integer.
/// @return status code
///
/// @param[out] out resulting integer
/// @param[in]  str string
/// @param[in]  min minimal allowed value (inclusive)
/// @param[in]  max maximal allowed value (inclusive)
bool
parse_uint64(uint64_t* out,
             const char* str,
             const uint64_t min,
             const uint64_t max)
{
  uintmax_t x;

  // Convert the input string into a number.
  errno = 0;
  x = strtoumax(str, NULL, 10);
  if (x == 0 && errno != 0) {
    log_(LL_ERROR, true, "main", "Unable to parse a number from string '%s'", str);
    return false;
  }

  // Verify that the number belongs to the specified range.
  if (x < (uintmax_t)min || x > (uintmax_t)max) {
    log_(LL_ERROR, false, "main", "Number %ju out of range "
           "(%" PRIu64 "..%" PRIu64 ")", x, min, max);
    return false;
  }

  *out = (uint64_t)x;
  return true;
}

/// Find the multiplier for the selected time unit for conversion to
/// nanoseconds.
///
/// @param[out] mult multiplier
/// @param[in]  unit unit abbreviation
void
parse_time_unit(uint64_t* mult, const char* unit)
{
  if (strcmp(unit, "ns") == 0) *mult = 1LL;
  if (strcmp(unit, "us") == 0) *mult = 1000LL;
  if (strcmp(unit, "ms") == 0) *mult = 1000LL * 1000;
  if (strcmp(unit, "s")  == 0) *mult = 1000LL * 1000 * 1000;
  if (strcmp(unit, "m")  == 0) *mult = 1000LL * 1000 * 1000 * 60;
  if (strcmp(unit, "h")  == 0) *mult = 1000LL * 1000 * 1000 * 60 * 60;
  if (strcmp(unit, "d")  == 0) *mult = 1000LL * 1000 * 1000 * 60 * 60 * 24;
}

/// Find the multiplier for the selected memory unit for conversion to bytes.
///
/// @param[out] mult multiplier
/// @param[in]  unit unit abbreviation
void
parse_memory_unit(uint64_t* mult, const char* unit)
{
  if (strcasecmp(unit, "b")  == 0) *mult = 1LL;
  if (strcasecmp(unit, "k")  == 0) *mult = 1024LL;
  if (strcasecmp(unit, "kb") == 0) *mult = 1024LL;
  if (strcasecmp(unit, "m")  == 0) *mult = 1024LL * 1024;
  if (strcasecmp(unit, "mb") == 0) *mult = 1024LL * 1024;
  if (strcasecmp(unit, "g")  == 0) *mult = 1024LL * 1024 * 1024;
  if (strcasecmp(unit, "gb") == 0) *mult = 1024LL * 1024 * 1024;
}

/// Parse a unit and a scalar from a string.
/// @return status code
///
/// @param[out] out scalar in the smallest unit
/// @param[in]  inp input string
/// @param[in]  sun smallest unit name
/// @param[in]  upf unit parser function
bool
parse_scalar(uint64_t* out,
             const char* inp,
             const char* sun,
             void (*upf) (uint64_t*, const char*))
{
  uint64_t num;
  uint64_t mult;
  uint64_t x;
  int n;
  int adv;
  char unit[3];

  memset(unit, '\0', sizeof(unit));

  // Separate the scalar and the unit of the input string.
  n = sscanf(inp, "%" SCNu64 "%2s%n", &num, unit, &adv);
  if (n == 0) {
    log_(LL_ERROR, false, "main", "Unable to parse the quantity in '%s'", inp);
    return false;
  }

  if (n == 1) {
    log_(LL_ERROR, false, "main", "No unit specified");
    return false;
  }

  // Verify that the full input string was parsed.
  if (adv != (int)strlen(inp)) {
    log_(LL_ERROR, false, "main",
           "The scalar string '%s' contains excess characters", inp);
    return false;
  }

  // Parse the unit of the input string.
  mult = 0;
  upf(&mult, unit);
  if (mult == 0) {
    log_(LL_ERROR, false, "Unknown unit '%2s'", unit);
    return false;
  }

  // Check for overflow of the value in the smallest unit.
  x = num * mult;
  if (x / mult != num) {
    log_(LL_ERROR, false, "main", "Quantity would overflow, maximum is %" PRIu64
           " %s", UINT64_MAX, sun);
    return false;
  }

  *out = x;
  return true;
}
