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
/// @param[out] pl payload
void
encode_payload(struct payload* pl)
{
  pl->pl_mgic  = htonl(pl->pl_mgic);
  pl->pl_port  = htons(pl->pl_port);
  pl->pl_snum  = htonll(pl->pl_snum);
  pl->pl_slen  = htonll(pl->pl_slen);
  pl->pl_laddr = htonll(pl->pl_laddr);
  pl->pl_haddr = htonll(pl->pl_haddr);
  pl->pl_reqk  = htonll(pl->pl_reqk);
  pl->pl_resk  = htonll(pl->pl_resk);
  pl->pl_mtm1  = htonll(pl->pl_mtm1);
  pl->pl_rtm1  = htonll(pl->pl_rtm1);
  pl->pl_mtm2  = htonll(pl->pl_mtm2);
  pl->pl_rtm2  = htonll(pl->pl_rtm2);
}

/// Decode the on-wire format of the payload. This function only deals with the
/// multi-byte fields, as all single-byte fields are correctly interpreted on
/// all endian encodings.
///
/// @param[out] pl payload
void
decode_payload(struct payload* pl)
{
  pl->pl_mgic  = ntohl(pl->pl_mgic);
  pl->pl_port  = ntohs(pl->pl_port);
  pl->pl_snum  = ntohll(pl->pl_snum);
  pl->pl_slen  = ntohll(pl->pl_slen);
  pl->pl_laddr = ntohll(pl->pl_laddr);
  pl->pl_haddr = ntohll(pl->pl_haddr);
  pl->pl_reqk  = ntohll(pl->pl_reqk);
  pl->pl_resk  = ntohll(pl->pl_resk);
  pl->pl_mtm1  = ntohll(pl->pl_mtm1);
  pl->pl_rtm1  = ntohll(pl->pl_rtm1);
  pl->pl_mtm2  = ntohll(pl->pl_mtm2);
  pl->pl_rtm2  = ntohll(pl->pl_rtm2);
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
