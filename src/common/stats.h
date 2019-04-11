// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_STATS_H
#define NEMO_COMMON_STATS_H

#include <stdint.h>


/// Statistics.
struct stats {
  uint64_t st_rall; ///< Number of overall received datagrams.
  uint64_t st_reni; ///< Received errors due to network issues.
  uint64_t st_resz; ///< Received errors due to size mismatch.
  uint64_t st_remg; ///< Received errors due to magic number mismatch.
  uint64_t st_repv; ///< Received errors due to payload version mismatch.
  uint64_t st_rety; ///< Received errors due to payload type.
  uint64_t st_sall; ///< Number of overall sent datagrams.
  uint64_t st_seni; ///< Sent errors due to network issues.
};

void reset_stats(struct stats* st);
void log_stats(const char* pn, const struct stats* st);

#endif
