// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_SOCKET_H
#define NEMO_COMMON_SOCKET_H

#include <stdbool.h>
#include <stdint.h>

#include "common/proto.h"


bool create_socket4(struct proto* pr,
                    const uint16_t port,
                    const uint64_t rbuf,
                    const uint64_t sbuf,
                    const uint8_t ttl);
bool create_socket6(struct proto* pr,
                    const uint16_t port,
                    const uint64_t rbuf,
                    const uint64_t sbuf,
                    const uint8_t hops);
bool get_assigned_port(uint16_t* pn, const struct proto* pr);
void delete_socket(const struct proto* pr);
void log_socket_port(const struct proto* pr);

#endif
