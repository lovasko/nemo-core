// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <inttypes.h>

#include "common/convert.h"
#include "common/log.h"
#include "common/payload.h"


/// Encode the payload to the on-wire format. This function only deals with the
/// multi-byte fields, as all single-byte fields are correctly interpreted on
/// all endian encodings.
///
/// @param[in] dst encoded payload
/// @param[in] src original payload
void
encode_payload(struct payload* dst, const struct payload* src)
{
  dst->pl_mgic  = htonl(src->pl_mgic);
  dst->pl_port  = htons(src->pl_port);
  dst->pl_snum  = htonll(src->pl_snum);
  dst->pl_slen  = htonll(src->pl_slen);
  dst->pl_laddr = htonll(src->pl_laddr);
  dst->pl_haddr = htonll(src->pl_haddr);
  dst->pl_reqk  = htonll(src->pl_reqk);
  dst->pl_resk  = htonll(src->pl_resk);
  dst->pl_mtm1  = htonll(src->pl_mtm1);
  dst->pl_rtm1  = htonll(src->pl_rtm1);
  dst->pl_mtm2  = htonll(src->pl_mtm2);
  dst->pl_rtm2  = htonll(src->pl_rtm2);
}

/// Decode the on-wire format of the payload. This function only deals with the
/// multi-byte fields, as all single-byte fields are correctly interpreted on
/// all endian encodings.
///
/// @param[in] dst decoded payload
/// @param[in] src original payload
void
decode_payload(struct payload* dst, const struct payload* src)
{
  dst->pl_mgic  = ntohl(src->pl_mgic);
  dst->pl_port  = ntohs(src->pl_port);
  dst->pl_snum  = ntohll(src->pl_snum);
  dst->pl_slen  = ntohll(src->pl_slen);
  dst->pl_laddr = ntohll(src->pl_laddr);
  dst->pl_haddr = ntohll(src->pl_haddr);
  dst->pl_reqk  = ntohll(src->pl_reqk);
  dst->pl_resk  = ntohll(src->pl_resk);
  dst->pl_mtm1  = ntohll(src->pl_mtm1);
  dst->pl_rtm1  = ntohll(src->pl_rtm1);
  dst->pl_mtm2  = ntohll(src->pl_mtm2);
  dst->pl_rtm2  = ntohll(src->pl_rtm2);
}

/// Verify the incoming payload for correctness.
/// @return success/failure indication
///
/// @param[out] st protocol event statistics 
/// @param[in]  n  length of the received data
/// @param[in]  pl payload
/// @param[in]  et expected payload type
bool
verify_payload(struct stats* st,
               const ssize_t n,
               const struct payload* pl,
               const uint8_t et)
{
  log(LL_TRACE, false, "verifying payload");

  // Verify the size of the datagram.
  if ((size_t)n != sizeof(*pl)) {
    log(LL_DEBUG, false, "wrong datagram size, expected: %zd, actual: %zu",
        sizeof(*pl), n);
    st->st_resz++;
    return false;
  }

  // Verify the magic identifier.
  if (pl->pl_mgic != NEMO_PAYLOAD_MAGIC) {
    log(LL_DEBUG, false, "payload identifier unknown, expected: %"
        PRIx32 ", actual: %" PRIx32, NEMO_PAYLOAD_MAGIC, pl->pl_mgic);
    st->st_remg++;
    return false;
  }

  // Verify the payload version.
  if (pl->pl_fver != NEMO_PAYLOAD_VERSION) {
    log(LL_DEBUG, false, "unsupported payload version, expected: %"
        PRIu8 ", actual: %" PRIu8, NEMO_PAYLOAD_VERSION, pl->pl_fver);
    st->st_repv++;
    return false;
  }

  // Verify the payload type.
  if (pl->pl_type != et) {
    log(LL_DEBUG, false, "unexpected payload type, expected: %"
        PRIu8 ", actual: %" PRIu8, et, pl->pl_type);
    st->st_rety++;
    return false;
  }

  return true;
}
