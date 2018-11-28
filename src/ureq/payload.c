// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <inttypes.h>

#include "common/convert.h"
#include "common/log.h"
#include "common/payload.h"
#include "common/ttl.h"
#include "ures/funcs.h"
#include "ures/types.h"


/// Verify the incoming payload for correctness.
/// @return success/failure indication
///
/// @param[out] pr protocol connection
/// @param[in]  n  length of the received data
/// @param[in]  pl payload
bool
verify_payload(struct proto* pr, const ssize_t n, const struct payload* pl)
{
  log(LL_TRACE, false, "main", "verifying payload");

  // Verify the size of the datagram.
  if ((size_t)n != sizeof(*pl)) {
    log(LL_DEBUG, false, "main", "wrong datagram size, expected: %zd, actual: %zu",
        sizeof(*pl), n);
    pr->pr_resz++;
    return false;
  }

  // Verify the magic identifier.
  if (pl->pl_mgic != NEMO_PAYLOAD_MAGIC) {
    log(LL_DEBUG, false, "main", "payload identifier unknown, expected: %"
        PRIx32 ", actual: %" PRIx32, NEMO_PAYLOAD_MAGIC, pl->pl_mgic);
    pr->pr_remg++;
    return false;
  }

  // Verify the payload version.
  if (pl->pl_fver != NEMO_PAYLOAD_VERSION) {
    log(LL_DEBUG, false, "main", "unsupported payload version, expected: %"
        PRIu8 ", actual: %" PRIu8, NEMO_PAYLOAD_VERSION, pl->pl_fver);
    pr->pr_repv++;
    return false;
  }

  // Verify the payload type.
  if (pl->pl_type != NEMO_PAYLOAD_TYPE_REQUEST) {
    log(LL_DEBUG, false, "main", "unexpected payload type, expected: %"
        PRIu8 ", actual: %" PRIu8, NEMO_PAYLOAD_TYPE_REQUEST, pl->pl_type);
    pr->pr_rety++;
    return false;
  }

  return true;
}