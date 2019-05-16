// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_UREQ_TYPES_H
#define NEMO_UREQ_TYPES_H

#include <stdint.h>
#include <stdbool.h>


#define PLUG_MAX 32
#define TARG_MAX 2048

/// Configuration.
struct config {
  const char* cf_pi[PLUG_MAX]; ///< Attached plugins.
  const char** cf_tg;          ///< Network targets.
  uint64_t    cf_ntg;          ///< Number of network targets.
  uint64_t    cf_cnt;          ///< Number of emitted payload rounds.
  uint64_t    cf_int;          ///< Inter-payload sleep interval.
  uint64_t    cf_wait;         ///< Wait time after last request.
  uint64_t    cf_rbuf;         ///< Socket receive buffer memory size.
  uint64_t    cf_sbuf;         ///< Socket send buffer memory size.
  uint64_t    cf_ttl;          ///< Time-To-Live for published datagrams.
  uint64_t    cf_key;          ///< Key of the current process.
  uint64_t    cf_port;         ///< UDP port for all endpoints.
  uint64_t    cf_rld;          ///< Name resolution refresh period.
  uint64_t    cf_len;          ///< Overall payload length.
  uint8_t     cf_llvl;         ///< Notification verbosity level.
  bool        cf_lcol;         ///< Notification coloring policy.
  bool        cf_err;          ///< Process exit policy on publishing error.
  bool        cf_mono;         ///< Do not capture responses (monologue mode).
  bool        cf_sil;          ///< Suppress reporting output.
  bool        cf_bin;          ///< Binary reporting mode.
  bool        cf_grp;          ///< Group requests at the beginning of a round.
  bool        cf_ipv4;         ///< IPv4-only mode.
  bool        cf_ipv6;         ///< IPv6-only mode.
};

/// Command-line option.
struct option {
  const char op_name;               ///< Name.
  bool op_arg;                      ///< Has argument?
  bool (*op_act)(struct config* cf, ///< Action to perform.
                 const char* inp);
};

/// Network endpoint.
struct target {
  const char* tg_name;   ///< Domain name.
  uint64_t    tg_laddr;  ///< Low address bits.
  uint64_t    tg_haddr;  ///< High address bits.
  uint8_t     tg_ipv;    ///< IP address version.
  uint8_t     tg_pad[7]; ///< Padding (unused).
};

#endif
