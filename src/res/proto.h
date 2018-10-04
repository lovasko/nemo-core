// Copyright (c) 2018 Daniel Lovasko.
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
#include "res/types.h"


// Loop.
bool respond_loop(int sock4, int sock6, const struct options* opts);

// Payload.
bool verify_payload(const ssize_t n, const payload* pl);
bool update_payload(payload* pl, struct msghdr* msg, const struct options* opts);

// Report.
void report_header(const struct options* opts);
void report_event(const payload* pl, const struct options* opts);
bool flush_report_stream(const struct options* opts);

// Options.
bool parse_options(struct options* opts, int argc, char* argv[]);
void log_options(const struct options* opts);

// Plugins.
bool load_plugins(struct plugin* pins, uint64_t* npins, const struct options* opts);
bool init_plugins(const struct plugin* pins, const uint64_t npins);
void exec_plugins(const struct plugin* pins, const uint64_t npins, const payload* pl);
void free_plugins(const struct plugin* pins, const uint64_t npins);

// Event.
bool handle_event(int sock, const char* ipv, const struct options* opts);

// Signal.
bool install_signal_handlers(void);
void block_all_signals(void);
void create_signal_mask(sigset_t* mask);

// Socket.
bool create_socket4(int* sock, const struct options* opts);
bool create_socket6(int* sock, const struct options* opts);

#endif
