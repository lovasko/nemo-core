// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_RES_TYPES_H
#define NEMO_RES_TYPES_H

#include <sys/types.h>

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
  bool        cf_err;            ///< Early exit on first network error.
  bool        cf_ipv4;           ///< IPv4-only traffic.
  bool        cf_ipv6;           ///< IPv6-only traffic.
  uint8_t     cf_llvl;           ///< Minimal log level.
  bool        cf_lcol;           ///< Log coloring policy.
  bool        cf_mono;           ///< Monologue mode (no responses).
  bool        cf_dmon;           ///< Daemon process.
  bool        cf_sil;            ///< Standard output presence.
  bool        cf_bin;            ///< Binary reporting mode.
  uint8_t     cf_pad[7];         ///< Padding (unused).
};

/// Command-line option.
struct option {
  const char op_name;               ///< Name.
  bool op_arg;                      ///< Has argument?
  bool (*op_act)(struct config* cf, ///< Action to perform.
                 const char* inp);
};

/// Event callback plugin.
struct plugin {
  const char* pi_name;                      ///< Name.
  void*       pi_hndl;                      ///< Shared object handle.
  bool      (*pi_init)(void);               ///< Initialisation procedure.
  bool      (*pi_evnt)(uint64_t, uint64_t,
                       uint64_t, uint64_t); ///< Response event procedure.
  bool      (*pi_free)(void);               ///< Clean-up procedure.
  pid_t       pi_pid;                       ///< Process ID of the sandbox.
  int         pi_pipe[2];                   ///< Payload notification channel.
};

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
  int         pr_pad;  ///< Structure padding (unused).
};

#endif
