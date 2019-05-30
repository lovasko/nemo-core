// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_RES_PROTO_H
#define NEMO_RES_PROTO_H

#include <sys/socket.h>

#include <stdlib.h>

#include "common/payload.h"
#include "common/plugin.h"
#include "common/proto.h"
#include "ures/types.h"


// Configuration.
bool parse_config(struct config* cf, int argc, char* argv[]);
void log_config(const struct config* cf);

// Event.
bool handle_event(struct proto* pr,
                  const struct plugin* pi,
                  const uint64_t npi,
                  const struct config* cf);

// Loop.
bool respond_loop(struct proto* pr,
                  struct plugin* pi,
                  const uint64_t npi,
                  const struct config* cf);

// Report.
void report_header(const struct config* cf);
void report_event(const struct payload* hpl,
                  const struct payload* npl,
                  const uint16_t port,
                  const struct config* cf);
bool flush_report_stream(const struct config* cf);

// Socket.
bool create_socket4(struct proto* pr, const struct config* cf);
bool create_socket6(struct proto* pr, const struct config* cf);
void delete_socket(const struct proto* pr);

#endif
