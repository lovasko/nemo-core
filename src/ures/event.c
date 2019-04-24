// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/socket.h>

#include "common/convert.h"
#include "common/log.h"
#include "common/now.h"
#include "common/ttl.h"
#include "common/payload.h"
#include "ures/funcs.h"
#include "ures/types.h"


/// Update payload with local diagnostic information.
/// @return success/failure indication
///
/// @param[out] pl  payload
/// @param[in]  msg message header
/// @param[in]  cf  configuration
static bool
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

/// Receive datagrams on both IPv4 and IPv6.
/// @return success/failure indication
///
/// @param[out] pr   protocol connection
/// @param[out] addr IPv4/IPv6 address
/// @param[out] pl   payload
/// @param[in]  msg  message headers
/// @param[in]  cf   configuration
static bool
receive_datagram(struct proto* pr,
                 struct sockaddr_storage* addr,
                 struct payload* pl,
                 struct msghdr* msg,
                 const struct config* cf)
{
  struct iovec data;
  ssize_t n;
  bool retb;

  log(LL_TRACE, false, "receiving datagram");

  // Prepare payload data.
  data.iov_base = pl;
  data.iov_len  = sizeof(*pl);

  // Prepare the message.
  msg->msg_name    = addr;
  msg->msg_namelen = sizeof(*addr);
  msg->msg_iov     = &data;
  msg->msg_iovlen  = 1;
  msg->msg_flags   = 0;

  // Receive the message and handle potential errors.
  pr->pr_stat.st_rall++;
  n = recvmsg(pr->pr_sock, msg, MSG_DONTWAIT | MSG_TRUNC);
  if (n < 0) {
    log(LL_WARN, true, "receiving has failed");
    pr->pr_stat.st_reni++;

    if (cf->cf_err == true) {
      return false;
    }
  }

  // Convert the payload from its on-wire format.
  decode_payload(pl);

  // Verify the payload correctness.
  retb = verify_payload(&pr->pr_stat, n, pl, NEMO_PAYLOAD_TYPE_REQUEST);
  if (retb == false) {
    log(LL_WARN, false, "invalid payload content");
    return false;
  }

  return true;
}

/// Send a payload to the defined address.
/// @return success/failure indication
///
/// @param[out] pr   protocol connection
/// @param[in]  pl   payload
/// @param[in]  addr IPv4/IPv6 address
/// @param[in]  cf   configuration
static bool
send_datagram(struct proto* pr,
              struct payload* pl,
              struct sockaddr_storage* addr,
              const struct config* cf)
{
  ssize_t n;
  struct msghdr msg;
  struct iovec data;

  log(LL_TRACE, false, "sending datagram");

  // Prepare payload data.
  data.iov_base = pl;
  data.iov_len  = sizeof(*pl);

  // Prepare the message.
  msg.msg_name       = addr;
  msg.msg_namelen    = sizeof(*addr);
  msg.msg_iov        = &data;
  msg.msg_iovlen     = 1;
  msg.msg_control    = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags      = 0;

  // Convert the payload to its on-wire format.
  encode_payload(pl);

  // Send the datagram.
  pr->pr_stat.st_sall++;
  n = sendmsg(pr->pr_sock, &msg, MSG_DONTWAIT);
  if (n < 0) {
    log(LL_WARN, true, "unable to send datagram");
    pr->pr_stat.st_seni++;

    if (cf->cf_err == true) {
      return false;
    }
  }

  // Verify the size of the sent datagram.
  if ((size_t)n != sizeof(*pl)) {
    log(LL_WARN, false, "wrong sent payload size");
    pr->pr_stat.st_seni++;

    if (cf->cf_err == true) {
      return false;
    }
  }

  return true;
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
  struct payload pl;
  struct msghdr msg;
  uint8_t cdata[512];

  log(LL_TRACE, false, "handling event on the %s socket", pr->pr_name);

  // Assign a buffer for storing the control messages received as part of the
  // datagram. This data is later passed to other functions for reporting the
  // relevant data.
  msg.msg_control    = cdata;
  msg.msg_controllen = sizeof(cdata);

  // Receive a request.
  retb = receive_datagram(pr, &addr, &pl, &msg, cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to receive datagram on the socket");

    // We do not continue with the response, given the receiving of the
    // request has failed. If the op_err option was selected, all network
    // transmission errors should be treated as fatal. In order to propagate
    // error, we need to return 'false', if op_err was 'true'. The same applies
    // in the opposite case.
    return !cf->cf_err;
  }

  // Update payload.
  retb = update_payload(&pl, &msg, cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to update the payload");
    return false;
  }

  // Notify all attached plugins about the payload.
  notify_plugins(pins, npins, &pl);

  // Report the event as a entry in the CSV output.
  report_event(&pl, cf);

  // Do not respond if the monologue mode is turned on.
  if (cf->cf_mono == true) {
    return true;
  }

  // Send a response back.
  retb = send_datagram(pr, &pl, &addr, cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to send datagram on the socket");

    // Following the same logic as above in the receive stage.
    return !cf->cf_err;
  }

  return true;
}
