// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_RES_TYPES_H
#define NEMO_RES_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define PLUG_MAX 32

/// Command-line options.
struct options {
  const char* op_plgs[PLUG_MAX]; ///< Paths to plugin shared object libraries.
  uint64_t    op_port;           ///< UDP port number.
  uint64_t    op_rbuf;           ///< Socket receive buffer size.
  uint64_t    op_sbuf;           ///< Socket send buffer size.
  uint64_t    op_key;            ///< Unique key.
  uint64_t    op_ttl;            ///< Time-To-Live for outgoing IP packets.
  bool        op_err;            ///< Early exit on first network error.
  bool        op_ipv4;           ///< IPv4-only traffic.
  bool        op_ipv6;           ///< IPv6-only traffic.
  uint8_t     op_llvl;           ///< Minimal log level.
  bool        op_lcol;           ///< Log coloring policy.
  bool        op_mono;           ///< Monologue mode (no responses).
  bool        op_dmon;           ///< Daemon process.
  bool        op_sil;            ///< Standard output presence.
};

/// Command-line option.
struct option {
  const char op_name;                  ///< Name.
  bool op_arg;                         ///< Has argument?
  bool (*op_act)(struct options* opts, ///< Action to perform.
                 const char* inp);
};

/// Plug-in shared object library.
struct plugin {
  const char* pi_name;                      ///< Name.
  void*       pi_hndl;                      ///< Shared object handle.
  bool      (*pi_init)(void);               ///< Initialisation procedure.
  bool      (*pi_evnt)(uint64_t, uint64_t,
                       uint64_t, uint64_t); ///< Response event procedure.
  bool      (*pi_free)(void);               ///< Clean-up procedure.
};

#endif
