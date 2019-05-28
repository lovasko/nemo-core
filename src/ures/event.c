// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/socket.h>

#include "common/convert.h"
#include "common/log.h"
#include "common/now.h"
#include "common/packet.h"
#include "common/payload.h"
#include "ures/funcs.h"
#include "ures/types.h"


/// Update payload with local diagnostic information.
/// @return success/failure indication
///
/// @param[in] pl  payload
/// @param[in] ttl Time-To-Live value
/// @param[in] cf  configuration
static void 
update_payload(struct payload* pl, const uint8_t ttl, const struct config* cf)
{
  log(LL_TRACE, false, "updating payload");

  pl->pl_type = NEMO_PAYLOAD_TYPE_RESPONSE;
  pl->pl_key = cf->cf_key;
  pl->pl_mtm2 = mono_now();
  pl->pl_rtm2 = real_now();
  pl->pl_ttl2 = ttl;
}

/// Retrieve the UDP port number from an address.
/// @return port number
///
/// @param[in] ss IPv4/IPv6 socket address
static uint16_t
retrieve_port(const struct sockaddr_storage* ss)
{
  struct sockaddr_in* sin;
  struct sockaddr_in6* sin6;
  
  // Cast the address to the appropriate format based on the address family.
  if (ss->ss_family == AF_INET) {
    sin = (struct sockaddr_in*)ss;
    return sin->sin_port;
  } else {
    sin6 = (struct sockaddr_in6*)ss;
    return sin6->sin6_port;
  }
}

/// Handle the event of an incoming datagram.
/// @return success/failure indication
///
/// @param[out] pr    protocol connection
/// @param[in]  pins  array of plugins
/// @param[in]  npins number of plugins
/// @param[in]  cf    configuration
bool
handle_event(struct proto* pr,
             const struct plugin* pins,
             const uint64_t npins,
             const struct config* cf)
{
  bool retb;
  struct sockaddr_storage addr;
  struct payload hpl;
  struct payload npl;
  uint8_t ttl;
  uint16_t port;

  log(LL_TRACE, false, "handling event on the %s socket", pr->pr_name);

  // Receive a request.
  retb = receive_packet(pr, &addr, &hpl, &npl, &ttl, cf->cf_err);
  if (retb == false) {
    log(LL_WARN, false, "unable to receive datagram on the socket");

    // We do not continue with the response, given the receiving of the
    // request has failed. If the cf_err option was selected, all network
    // transmission errors should be treated as fatal. In order to propagate
    // error, we need to return 'false', if cf_err was 'true'. The same applies
    // in the opposite case.
    return !cf->cf_err;
  }

  // Retrieve the port of the requester.
  port = retrieve_port(&addr);

  // Do not respond if a particular key is selected, and the requesters key
  // does not match.
  if (cf->cf_key != 0 && (hpl.pl_key != cf->cf_key)) {
    return true;
  }

  // Do not respond if the overall length of the packet does not match the
  // expected length.
  if (cf->cf_len != 0 && (hpl.pl_len != cf->cf_len)) {
    return true;
  }

  // Update payload.
  update_payload(&hpl, ttl, cf);

  // Report the event as a entry in the CSV output.
  report_event(&hpl, &npl, port, cf);

  // Notify all attached plugins about the payload.
  notify_plugins(pins, npins, &hpl);

  // Do not respond if the monologue mode is turned on.
  if (cf->cf_mono == true) {
    return true;
  }

  // Send a response back.
  retb = send_packet(pr, &hpl, addr, cf->cf_err);
  if (retb == false) {
    log(LL_WARN, false, "unable to send datagram on the socket");

    // Following the same logic as above in the receive stage.
    return !cf->cf_err;
  }

  return true;
}
