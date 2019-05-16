// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_PAYLOAD_H
#define NEMO_COMMON_PAYLOAD_H

#include <stdint.h>


// Message types.
#define NEMO_PAYLOAD_TYPE_RESPONSE 1
#define NEMO_PAYLOAD_TYPE_REQUEST  2

// Constants.
#define NEMO_PAYLOAD_MAGIC 0x6e656d6f
#define NEMO_PAYLOAD_VERSION        4

// Internet protocol versions.
#define NEMO_IP_VERSION_4 4
#define NEMO_IP_VERSION_6 6

// Memory size.
#define NEMO_PAYLOAD_SIZE 88

// Payload.
struct payload {
  uint32_t pl_mgic;   ///< Magic identifier.
  uint8_t  pl_fver;   ///< Format version.
  uint8_t  pl_type;   ///< Message type.
  uint16_t pl_port;   ///< UDP port.
  uint8_t  pl_ttl1;   ///< Time-To-Live when sent from requester.
  uint8_t  pl_ttl2;   ///< Time-To-Live when received by responder.
  uint8_t  pl_ttl3;   ///< Time-To-Live when sent from responder.
  uint8_t  pl_pver;   ///< IP protocol version.
  uint16_t pl_len;    ///< Artificial payload length in bytes.
  uint8_t  pl_pad[2]; ///< Padding (unused).
  uint64_t pl_snum;   ///< Sequence iteration number.
  uint64_t pl_slen;   ///< Sequence length.
  uint64_t pl_laddr;  ///< IP address - low-bits.
  uint64_t pl_haddr;  ///< IP address - high-bits.
  uint64_t pl_key;    ///< Responder/requester key.
  uint64_t pl_mtm1;   ///< Steady time of request.
  uint64_t pl_rtm1;   ///< System time of request.
  uint64_t pl_mtm2;   ///< Steady time of response.
  uint64_t pl_rtm2;   ///< System time of response.
};

#endif
