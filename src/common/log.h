// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_UTIL_LOG_H
#define NEMO_UTIL_LOG_H

#include <stdbool.h>
#include <stdint.h>


// Logging levels.
#define LL_ERROR 0 // Error.
#define LL_WARN  1 // Warning.
#define LL_INFO  2 // Information.
#define LL_DEBUG 3 // Debug.
#define LL_TRACE 4 // Tracing.

// Logging back-end service.
#define LB_STDERR 1 // Standard error stream.
#define LB_SYSLOG 2 // System logging daemon.

// Logging settings.
extern uint8_t log_lvl; ///< Minimal level threshold.
extern bool log_col;    ///< Colouring policy.

// Logging function.
void log(const uint8_t lvl,
         const bool perr,
         const char* name,
         const char* msg,
         ...);

#endif
