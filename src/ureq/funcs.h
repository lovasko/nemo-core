// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

#include "common/payload.h"
#include "ureq/types.h"


// Configuration.
bool parse_config(struct config* cf, int argc, char* argv[]);
void log_config(const struct config* cf, const struct proto* p4, const struct proto* p6);

// Counters.
void reset_counters(struct proto* pr);
void log_counters(const struct proto* pr);

// Loop.
bool request_loop(struct proto* p4,
                  struct proto* p6,
                  struct target* tg,
                  const uint64_t tmax,
                  const struct config* cf);

// Payload.
bool verify_payload(const struct proto* pr, const ssize_t n, const struct payload* pl);

// Report.
void report_header(const struct config* cf);
void report_event(const struct payload* pl, const struct config* cf);
bool flush_report_stream(const struct config* cf);

// Signal.
bool install_signal_handlers(void);
void create_signal_mask(sigset_t* mask);

// Socket.
bool create_socket4(struct proto* pr, const struct config* cf);
bool create_socket6(struct proto* pr, const struct config* cf);
bool get_assigned_port(uint16_t* pn, const struct proto* pr);

// Target.
void log_targets(const struct target tg[], const uint64_t cnt);
bool load_targets(struct target* tg,
                  uint64_t* cnt,
                  const uint64_t tmax,
                  const struct config* cf);
