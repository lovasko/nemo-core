// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_RES_TYPES_H
#define NEMO_RES_TYPES_H

#include <stdint.h>
#include <stdbool.h>


#define PLUG_MAX 32

/// Configuration.
struct config {
  const char* cf_plgs[PLUG_MAX]; ///< Paths to plugin shared object libraries.
  uint64_t    cf_port;           ///< UDP port number.
  uint64_t    cf_rbuf;           ///< Socket receive buffer size.
  uint64_t    cf_sbuf;           ///< Socket send buffer size.
  uint64_t    cf_key;            ///< Unique key.
  uint64_t    cf_ttl;            ///< Time-To-Live for outgoing IP packets.
  uint64_t    cf_ito;            ///< Inactivity timeout.
  uint64_t    cf_len;            ///< Overall packet length.
  bool        cf_err;            ///< Early exit on first network error.
  bool        cf_ipv4;           ///< Usage of Internet Protocol version 4.
  uint8_t     cf_llvl;           ///< Minimal log level.
  bool        cf_lcol;           ///< Log coloring policy.
  bool        cf_mono;           ///< Monologue mode (no responses).
  bool        cf_sil;            ///< Standard output presence.
  uint8_t     cf_pad[2];         ///< Padding (unused).
};

/// Command-line option.
struct option {
  const char op_name;               ///< Name.
  bool op_arg;                      ///< Has argument?
  bool (*op_act)(struct config* cf, ///< Action to perform.
                 const char* inp);
};

#endif
