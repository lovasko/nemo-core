// Copyright (c) 2018-2019 Daniel Lovasko
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

#include "common/log.h"
#include "common/parse.h"


/// Verify that the value belongs to the specified range.
/// @return interval membership
///
/// @param[in] val value to check
/// @param[in] min lower inclusive bound
/// @param[in] max upper inclusive bound
static bool
check_bounds(const uint64_t val, const uint64_t min, const uint64_t max)
{
  if (val < min) {
    log(LL_WARN, false, "value %" PRIu64 " is below lower inclusive bound of %" PRIu64, val, min);
    return false;
  }

  if (val > max) {
    log(LL_WARN, false, "value %" PRIu64 " is above upper inclusive bound of %" PRIu64, val, max);
    return false;
  }

  return true;
}

/// Convert a string into an unsigned 64-bit integer.
/// @return status code
///
/// @param[out] out resulting integer
/// @param[in]  str string
/// @param[in]  min lower inclusive bound
/// @param[in]  max upper inclusive bound
bool
parse_uint64(uint64_t* out,
             const char* str,
             const uint64_t min,
             const uint64_t max)
{
  uintmax_t x;
  bool retb;

  // Convert the input string into a number.
  errno = 0;
  x = strtoumax(str, NULL, 10);
  if (x == 0 && errno != 0) {
    log(LL_ERROR, true, "unable to parse a number from string '%s'", str);
    return false;
  }

  // Verify that the number belongs to the specified range.
  retb = check_bounds((uint64_t)x, min, max);
  if (retb == false) {
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
bool
parse_time_unit(uint64_t* mult, const char* unit)
{
  int reti;

  // Nanoseconds.
  reti = strcasecmp(unit, "ns");
  if (reti == 0) {
    *mult = 1LL;
    return true;
  }

  // Microseconds.
  reti = strcasecmp(unit, "us");
  if (reti == 0) {
    *mult = 1000LL;
    return true;
  }

  // Milliseconds.
  reti = strcasecmp(unit, "ms");
  if (reti == 0) {
    *mult = 1000LL * 1000;
    return true;
  }

  // Seconds.
  reti = strcasecmp(unit, "s");
  if (reti == 0) {
    *mult = 1000LL * 1000 * 1000;
    return true;
  }

  // Minutes.
  reti = strcasecmp(unit, "m");
  if (reti == 0) {
    *mult = 1000LL * 1000 * 1000 * 60;
    return true;
  }

  // Hours.
  reti = strcasecmp(unit, "h");
  if (reti == 0) {
    *mult = 1000LL * 1000 * 1000 * 60 * 60;
    return true;
  }

  // Days.
  reti = strcasecmp(unit, "d");
  if (reti == 0) {
    *mult = 1000LL * 1000 * 1000 * 60 * 60 * 24;
    return true;
  }

  // Weeks.
  reti = strcasecmp(unit, "w");
  if (reti == 0) {
    *mult = 1000LL * 1000 * 1000 * 60 * 60 * 24 * 7;
    return true;
  }

  return false;
}

/// Find the multiplier for the selected memory unit for conversion to bytes.
///
/// @param[out] mult multiplier
/// @param[in]  unit unit abbreviation
bool
parse_memory_unit(uint64_t* mult, const char* unit)
{
  int reti1;
  int reti2;

  // Bytes.
  reti1 = strcasecmp(unit, "b");
  if (reti1 == 0) {
    *mult = 1LL;
    return true;
  }

  // Kilobytes.
  reti1 = strcasecmp(unit, "k");
  reti2 = strcasecmp(unit, "kb");
  if (reti1 == 0 || reti2 == 0) {
    *mult = 1024LL;
    return true;
  }

  // Megabytes.
  reti1 = strcasecmp(unit, "m");
  reti2 = strcasecmp(unit, "mb");
  if (reti1 == 0 || reti2 == 0) {
    *mult = 1024LL * 1024;
    return true;
  }

  // Gigabytes.
  reti1 = strcasecmp(unit, "g");
  reti2 = strcasecmp(unit, "gb");
  if (reti1 == 0 || reti2 == 0) {
    *mult = 1024LL * 1024 * 1024;
    return true;
  }

  return false;
}

/// Parse a unit and a scalar from a string.
/// @return status code
///
/// @param[out] out  scalar in the smallest unit
/// @param[in]  inp  input string
/// @param[in]  name smallest unit name
/// @param[in]  min lower inclusive bound
/// @param[in]  max upper inclusive bound
/// @param[in]  func unit parser function
bool
parse_scalar(uint64_t* out,
             const char* inp,
             const char* name,
             const uint64_t min,
             const uint64_t max,
             bool (*func) (uint64_t*, const char*))
{
  size_t len;
  uint64_t num;
  uint64_t mult;
  uint64_t x;
  int n;
  int adv;
  char unit[3];
  bool retb;

  (void)memset(unit, '\0', sizeof(unit));

  // Separate the scalar and the unit of the input string.
  n = sscanf(inp, "%" SCNu64 "%2s%n", &num, unit, &adv);
  if (n == 0) {
    log(LL_ERROR, false, "unable to parse the quantity in '%s'", inp);
    return false;
  }

  if (n == 1) {
    log(LL_ERROR, false, "no unit specified");
    return false;
  }

  // Verify that the full input string was parsed.
  len = strlen(inp);
  if (adv != (int)len) {
    log(LL_ERROR, false, "scalar string '%s' contains excess characters", inp);
    return false;
  }

  // Parse the unit of the input string.
  retb = func(&mult, unit);
  if (retb == false) {
    log(LL_ERROR, false, "unknown unit '%2s'", unit);
    return false;
  }

  // Check for overflow of the value in the smallest unit.
  x = num * mult;
  if (x / mult != num) {
    log(LL_ERROR, false, "quantity would overflow", name);
    return false;
  }

  // Verify that the value falls within the selected bounds.
  retb = check_bounds(x, min, max);
  if (retb == false) {
    return false;
  }

  *out = x;
  return true;
}
