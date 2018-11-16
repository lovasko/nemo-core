// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_RES_PROTO_H
#define NEMO_RES_PROTO_H

#include <sys/socket.h>

#include <stdlib.h>
#include <signal.h>

#include "common/payload.h"
#include "ures/types.h"


// Configuration.
bool parse_config(struct config* cf, int argc, char* argv[]);
void log_config(const struct config* cf);

// Counters.
void reset_counters(struct proto* pr);
void log_counters(const struct proto* pr);

// Event.
bool handle_event(struct proto* pr,
                  const struct plugin* pins,
                  const uint64_t npins,
                  const struct config* cf);

// Loop.
bool respond_loop(struct proto* p4,
                  struct proto* p6,
                  const struct plugin* pins,
                  const uint64_t npins,
                  const struct config* cf);
  
// Payload.
bool verify_payload(struct proto* pr,
                    const ssize_t n,
                    const struct payload* pl);
bool update_payload(struct payload* pl,
                    struct msghdr* msg,
                    const struct config* cf);

// Report.
void report_header(const struct config* cf);
void report_event(const struct payload* pl, const struct config* cf);
bool flush_report_stream(const struct config* cf);

// Plugins.
bool load_plugins(struct plugin* pins,
                  uint64_t* npins,
                  const struct config* cf);
bool start_plugins(struct plugin* pins, const uint64_t npins);
void notify_plugins(const struct plugin* pins,
                    const uint64_t npins,
                    const struct payload* pl);
void terminate_plugins(const struct plugin* pins, const uint64_t npins);

// Signal.
bool install_signal_handlers(void);
void create_signal_mask(sigset_t* mask);

// Socket.
bool create_socket4(struct proto* pr, const struct config* cf);
bool create_socket6(struct proto* pr, const struct config* cf);
void delete_socket(const struct proto* pr);

#endif
