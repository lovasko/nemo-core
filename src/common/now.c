// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <unistd.h>
#include <stdint.h>
#include <time.h>

#include "common/convert.h"
#include "common/now.h"


// Make sure that the POSIX timers are available on this system.
#if _POSIX_TIMERS == 0
  #error "POSIX timers not available on this system"
#endif

// Make sure that the monotonic clock is available on this system.
#ifndef _POSIX_MONOTONIC_CLOCK
  #error "POSIX monotonic timer not available on this system"
#endif


/// Get the current real-time clock value.
/// @return time in nanoseconds
uint64_t
real_now(void)
{
  uint64_t ns;
  struct timespec ts;

  // This function ignores the return value of the clock call, as none of the
  // possible error conditions apply:
  //   EFAULT: timespec storage is on the local stack, and therefore valid
  //   EINVAL: real-time clock is guaranteed to exist by the POSIX standard
  (void)clock_gettime(CLOCK_REALTIME, &ts);
  tnanos(&ns, ts);

  return ns;
}

/// Get the current monotonic clock value.
/// @return time in nanoseconds
uint64_t
mono_now(void)
{
  uint64_t ns;
  struct timespec ts;

  // This function ignores the return value of the clock call, as none of the
  // possible error conditions apply:
  //   EFAULT: timespec storage is on the local stack, and therefore valid
  //   EINVAL: presence of the monotonic clock is verified by ifndef above
  (void)clock_gettime(CLOCK_MONOTONIC, &ts);
  tnanos(&ns, ts);

  return ns;
}
