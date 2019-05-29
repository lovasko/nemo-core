// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdlib.h>
#include <stdint.h>

#include "common/payload.h"
#include "common/proto.h"
#include "ureq/types.h"


// Configuration.
bool parse_config(struct config* cf, int argc, char* argv[]);
void log_config(const struct config* cf);

// Event.
bool wait_for_events(struct proto* pr, const uint64_t dur, const struct config* cf);

// Loop.
bool request_loop(struct proto* pr, struct target* tg, const struct config* cf);

// Report.
void report_header(const struct config* cf);
void report_event(const struct payload* hpl,
                  const struct payload* npl,
                  const uint64_t real,
                  const uint64_t mono, 
                  const uint8_t ttl,
                  const struct config* cf);
bool flush_report_stream(const struct config* cf);

// Round.
bool dispersed_round(struct proto* pr,
                     const struct target* tg,
                     const uint64_t ntg,
                     const uint64_t snum,
                     const struct config* cf);
bool grouped_round(struct proto* pr,
                   const struct target* tg,
                   const uint64_t ntg,
                   const uint64_t snum,
                   const struct config* cf);

// Socket.
bool create_socket4(struct proto* pr, const struct config* cf);
bool create_socket6(struct proto* pr, const struct config* cf);
bool get_assigned_port(uint16_t* pn, const struct proto* pr);
void log_socket_port(const struct proto* pr);

// Target.
void log_targets(const struct target tg[], const uint64_t cnt, const struct config* cf);
bool load_targets(struct target* tg, uint64_t* cnt, const struct config* cf);
