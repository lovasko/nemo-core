// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <inttypes.h>

#include "common/convert.h"
#include "common/log.h"
#include "common/now.h"
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
  log(LL_TRACE, false, "verifying payload");

  // Verify the size of the datagram.
  if ((size_t)n != sizeof(*pl)) {
    log(LL_DEBUG, false, "wrong datagram size, expected: %zd, actual: %zu",
        sizeof(*pl), n);
    pr->pr_resz++;
    return false;
  }

  // Verify the magic identifier.
  if (pl->pl_mgic != NEMO_PAYLOAD_MAGIC) {
    log(LL_DEBUG, false, "payload identifier unknown, expected: %"
        PRIx32 ", actual: %" PRIx32, NEMO_PAYLOAD_MAGIC, pl->pl_mgic);
    pr->pr_remg++;
    return false;
  }

  // Verify the payload version.
  if (pl->pl_fver != NEMO_PAYLOAD_VERSION) {
    log(LL_DEBUG, false, "unsupported payload version, expected: %"
        PRIu8 ", actual: %" PRIu8, NEMO_PAYLOAD_VERSION, pl->pl_fver);
    pr->pr_repv++;
    return false;
  }

  // Verify the payload type.
  if (pl->pl_type != NEMO_PAYLOAD_TYPE_REQUEST) {
    log(LL_DEBUG, false, "unexpected payload type, expected: %"
        PRIu8 ", actual: %" PRIu8, NEMO_PAYLOAD_TYPE_REQUEST, pl->pl_type);
    pr->pr_rety++;
    return false;
  }

  return true;
}

/// Update payload with local diagnostic information.
/// @return success/failure indication
///
/// @param[out] pl  payload
/// @param[in]  msg message header
/// @param[in]  cf  configuration
bool
update_payload(struct payload* pl, struct msghdr* msg, const struct config* cf)
{
  int ttl;
  bool retb;

  log(LL_TRACE, false, "updating payload");

  // Change the message type.
  pl->pl_type = NEMO_PAYLOAD_TYPE_RESPONSE;

  // Sign the payload.
  pl->pl_resk = cf->cf_key;

  // Obtain the current monotonic clock value.
  pl->pl_mtm2 = mono_now();

  // Obtain the current real-time clock value.
  pl->pl_rtm2 = real_now();

  // Retrieve the received Time-To-Live value.
  retb = retrieve_ttl(&ttl, msg);
  if (retb == false) {
    log(LL_WARN, false, "unable to extract IP Time-To-Live value");
    pl->pl_ttl2 = 0;
  } else {
    pl->pl_ttl2 = (uint8_t)ttl;
  }

  return true;
}
