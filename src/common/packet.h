// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_PACKET_H
#define NEMO_COMMON_PACKET_H

#include <sys/socket.h>

#include <stdint.h>
#include <stdbool.h>

#include "common/payload.h"
#include "common/proto.h"


bool send_packet(struct proto* pr,
                 const struct payload* hpl,
                 const struct sockaddr_storage addr,
                 const bool err);
bool receive_packet(struct proto* pr,
                    struct sockaddr_storage* addr,
                    struct payload* hpl,
                    struct payload* npl,
                    uint8_t* ttl,
                    const bool err);

#endif
