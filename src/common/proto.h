// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_PROTO_H
#define NEMO_COMMON_PROTO_H

#include <stdint.h>


/// Internet protocol connection.
struct proto {
  uint64_t    pr_rall; ///< Number of overall received datagrams.
  uint64_t    pr_reni; ///< Received errors due to network issues.
  uint64_t    pr_resz; ///< Received errors due to size mismatch.
  uint64_t    pr_remg; ///< Received errors due to magic number mismatch.
  uint64_t    pr_repv; ///< Received errors due to payload version mismatch.
  uint64_t    pr_rety; ///< Received errors due to payload type.
  uint64_t    pr_sall; ///< Number of overall sent datagrams.
  uint64_t    pr_seni; ///< Sent errors due to network issues.
  const char* pr_name; ///< Human-readable name.
  int         pr_sock; ///< Network socket.
  int         pr_ipv;  ///< Protocol version (4 or 6).
};

#endif
