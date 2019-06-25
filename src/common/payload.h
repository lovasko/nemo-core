// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_PAYLOAD_H
#define NEMO_COMMON_PAYLOAD_H

#include <stdint.h>


// Message types.
#define NEMO_PAYLOAD_TYPE_REQUEST  0
#define NEMO_PAYLOAD_TYPE_RESPONSE 1

// Constants.
#define NEMO_PAYLOAD_MAGIC   0x444c
#define NEMO_PAYLOAD_VERSION      6

// Memory size.
#define NEMO_PAYLOAD_SIZE  120
#define NEMO_HOST_NAME_SIZE 36
 
// Payload.
struct payload {
  uint16_t pl_mgic;     ///< Magic identifier.
  uint16_t pl_len;      ///< Artificial payload length in bytes.
  uint8_t  pl_fver : 5; ///< Format version.
  uint8_t  pl_type : 1; ///< Message type.
  uint8_t  pl_pad  : 2; ///< Padding (unused).
  uint8_t  pl_ttl1;     ///< Time-To-Live when sent from requester.
  uint8_t  pl_ttl2;     ///< Time-To-Live when received by responder.
  uint8_t  pl_ttl3;     ///< Time-To-Live when sent from responder.
  uint64_t pl_snum;     ///< Sequence iteration number.
  uint64_t pl_slen;     ///< Sequence length.
  uint64_t pl_laddr;    ///< IP address - low-bits.
  uint64_t pl_haddr;    ///< IP address - high-bits.
  uint64_t pl_key;      ///< Responder/requester key.
  uint64_t pl_mtm1;     ///< Steady time of request.
  uint64_t pl_rtm1;     ///< System time of request.
  uint64_t pl_mtm2;     ///< Steady time of response.
  uint64_t pl_rtm2;     ///< System time of response.
  char     pl_host[NEMO_HOST_NAME_SIZE]; ///< Host name.
};

#endif
