// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_PROTO_H
#define NEMO_COMMON_PROTO_H

#include "common/stats.h"


/// Internet protocol connection.
struct proto {
  struct stats pr_stat; ///< Event counter statistics.
  const char*  pr_name; ///< Human-readable name.
  int          pr_sock; ///< Network socket.
};

#endif
