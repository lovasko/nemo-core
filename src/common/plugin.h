// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_PLUGIN_H
#define NEMO_COMMON_PLUGIN_H

#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>

#include "common/payload.h"


#define PLUG_MAX 32

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

bool load_plugins(struct plugin* pi, uint64_t* npi, const char* so[]);
bool start_plugins(struct plugin* pi, const uint64_t npi);
void terminate_plugins(const struct plugin* pi, const uint64_t npi);
void notify_plugins(const struct plugin* pi,
                    const uint64_t npi,
                    const struct payload* pl);
void log_plugins(const struct plugin* pi, const uint64_t npi);

#endif
