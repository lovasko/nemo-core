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
#include "common/channel.h"


bool send_packet(struct channel* ch,
                 const struct payload* pl,
                 const struct sockaddr_storage addr,
                 const bool err);
bool receive_packet(struct channel* ch,
                    struct sockaddr_storage* addr,
                    struct payload* pl,
                    uint8_t* ttl,
                    const bool err);

#endif
