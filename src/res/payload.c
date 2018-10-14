// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <inttypes.h>

#include "common/payload.h"
#include "res/proto.h"
#include "res/types.h"
#include "util/convert.h"
#include "util/log.h"
#include "util/ttl.h"


/// Verify the incoming payload for correctness.
/// @return success/failure indication
///
/// @param[out] cts event counters
/// @param[in]  n   length of the received data
/// @param[in]  pl  payload
bool
verify_payload(struct counters* cts, const ssize_t n, const payload* pl)
{
  log(LL_TRACE, false, "main", "verifying payload");

  // Verify the size of the datagram.
  if ((size_t)n != sizeof(*pl)) {
    log(LL_DEBUG, false, "main", "wrong datagram size, expected: %zd, actual: %zu",
        sizeof(*pl), n);
    cts->ct_resz++;
    return false;
  }

  // Verify the magic identifier.
  if (pl->pl_mgic != NEMO_PAYLOAD_MAGIC) {
    log(LL_DEBUG, false, "main", "payload identifier unknown, expected: %"
        PRIx32 ", actual: %" PRIx32, NEMO_PAYLOAD_MAGIC, pl->pl_mgic);
    cts->ct_remg++;
    return false;
  }

  // Verify the payload version.
  if (pl->pl_fver != NEMO_PAYLOAD_VERSION) {
    log(LL_DEBUG, false, "main", "unsupported payload version, expected: %"
        PRIu8 ", actual: %" PRIu8, NEMO_PAYLOAD_VERSION, pl->pl_fver);
    cts->ct_repv++;
    return false;
  }

  // Verify the payload type.
  if (pl->pl_type != NEMO_PAYLOAD_TYPE_REQUEST) {
    log(LL_DEBUG, false, "main", "unexpected payload type, expected: %"
        PRIu8 ", actual: %" PRIu8, NEMO_PAYLOAD_TYPE_REQUEST, pl->pl_type);
    cts->ct_rety++;
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
update_payload(payload* pl, struct msghdr* msg, const struct config* cf)
{
  struct timespec mts;
  struct timespec rts;
  int reti;
  int ttl;
  bool retb;

  log(LL_TRACE, false, "main", "updating payload");

  // Change the message type.
  pl->pl_type = NEMO_PAYLOAD_TYPE_RESPONSE;

  // Sign the payload.
  pl->pl_resk = cf->cf_key;

  // Obtain the steady (monotonic) clock time.
  reti = clock_gettime(CLOCK_MONOTONIC, &mts);
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to obtain the steady time");
    return false;
  }
  tnanos(&pl->pl_mtm2, mts);

  // Obtain the system (real-time) clock time.
  reti = clock_gettime(CLOCK_REALTIME, &rts);
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to obtain the system time");
    return false;
  }
  tnanos(&pl->pl_rtm2, rts);

  // Retrieve the received Time-To-Live value.
  retb = retrieve_ttl(&ttl, msg);
  if (retb == false) {
    log(LL_WARN, false, "main", "unable to extract IP Time-To-Live value");
    pl->pl_ttl2 = 0;
  } else {
    pl->pl_ttl2 = (uint8_t)ttl;
  }

  return true;
}
