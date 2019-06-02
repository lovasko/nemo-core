// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_CHANNEL_H
#define NEMO_COMMON_CHANNEL_H

#include <stdbool.h>
#include <stdint.h>


/// Communication channel.
struct channel {
  uint64_t    ch_rall;   ///< Number of overall received datagrams.
  uint64_t    ch_reni;   ///< Received errors due to network issues.
  uint64_t    ch_resz;   ///< Received errors due to size mismatch.
  uint64_t    ch_remg;   ///< Received errors due to magic number mismatch.
  uint64_t    ch_repv;   ///< Received errors due to payload version mismatch.
  uint64_t    ch_rety;   ///< Received errors due to payload type.
  uint64_t    ch_sall;   ///< Number of overall sent datagrams.
  uint64_t    ch_seni;   ///< Sent errors due to network issues.
  const char* ch_name;   ///< Human-readable name.
  int         ch_sock;   ///< Network socket.
  uint16_t    ch_port;   ///< Local UDP port.
  uint8_t     ch_pad[2]; ///< Padding (unused).
};

bool open_channel4(struct channel* ch,
                   const uint16_t port,
                   const uint64_t rbuf,
                   const uint64_t sbuf,
                   const uint8_t ttl);
bool open_channel6(struct channel* ch,
                   const uint16_t port,
                   const uint64_t rbuf,
                   const uint64_t sbuf,
                   const uint8_t hops);
void log_channel(const struct channel* ch);
void close_channel(const struct channel* ch);

#endif
