// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/socket.h>

#include "common/convert.h"
#include "common/log.h"
#include "common/payload.h"
#include "res/proto.h"
#include "res/types.h"


/// Receive datagrams on both IPv4 and IPv6.
/// @return success/failure indication
///
/// @param[out] cts  event counters
/// @param[out] addr IPv4/IPv6 address
/// @param[out] pl   payload
/// @param[in]  msg  message headers
/// @param[in]  sock socket
/// @param[in]  cf   configuration
static bool
receive_datagram(struct counters* cts,
                 struct sockaddr_storage* addr,
                 struct payload* pl,
                 struct msghdr* msg,
                 int sock,
                 const struct config* cf)
{
  struct iovec data;
  ssize_t n;
  bool retb;

  log(LL_TRACE, false, "main", "receiving datagram");

  // Prepare payload data.
  data.iov_base = pl;
  data.iov_len  = sizeof(*pl);

  // Prepare the message.
  msg->msg_name       = addr;
  msg->msg_namelen    = sizeof(*addr);
  msg->msg_iov        = &data;
  msg->msg_iovlen     = 1;

  // Receive the message and handle potential errors.
  n = recvmsg(sock, msg, MSG_DONTWAIT | MSG_TRUNC);
  if (n < 0) {
    log(LL_WARN, true, "main", "receiving has failed");
    cts->ct_reni++;

    if (cf->cf_err == true)
      return false;
  }

  // Convert the payload from its on-wire format.
  decode_payload(pl);

  // Verify the payload correctness.
  retb = verify_payload(cts, n, pl);
  if (retb == false) {
    log(LL_WARN, false, "main", "invalid payload content");
    return false;
  }

  return true;
}

/// Send a payload to the defined address.
/// @return success/failure indication
///
/// @param[out] cts  event counters
/// @param[in]  sock socket
/// @param[in]  pl   payload
/// @param[in]  addr IPv4/IPv6 address
/// @param[in]  cf   configuration
static bool
send_datagram(struct counters* cts,
              int sock,
              struct payload* pl,
              struct sockaddr_storage* addr,
              const struct config* cf)
{
  ssize_t n;
  struct msghdr msg;
  struct iovec data;

  log(LL_TRACE, false, "main", "sending datagram");

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

  // Convert the payload to its on-wire format.
  encode_payload(pl);

  // Send the datagram.
  n = sendmsg(sock, &msg, MSG_DONTWAIT);
  if (n < 0) {
    log(LL_WARN, true, "main", "unable to send datagram");
    cts->ct_seni++;

    if (cf->cf_err == true)
      return false;
  }

  // Verify the size of the sent datagram.
  if ((size_t)n != sizeof(*pl)) {
    log(LL_WARN, false, "main", "wrong sent payload size");

    if (cf->cf_err == true)
      return false;
  }

  return true;
}

/// Handle the event of an incoming datagram.
/// @return success/failure indication
///
/// @param[out] cts  event counters
/// @param[in]  sock socket
/// @param[in]  ipv  IP version
/// @param[in]  cf   configuration
bool
handle_event(struct counters* cts,
             int sock,
             const char* ipv,
             const struct config* cf)
{
  bool retb;
  struct sockaddr_storage addr;
  struct payload pl;
  struct msghdr msg;
  uint8_t cdata[512];

  // Assign a buffer for storing the control messages received as part of the
  // datagram. This data is later passed to other functions for reporting the
  // relevant data.
  msg.msg_control    = cdata;
  msg.msg_controllen = sizeof(cdata);

  // Receive a request.
  retb = receive_datagram(cts, &addr, &pl, &msg, sock, cf);
  if (retb == false) {
    log(LL_WARN, false, "main", "unable to receive datagram on the %s socket", ipv);

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
    log(LL_WARN, false, "main", "unable to update the payload");
    return false;
  }

  // Trigger all associated action sets.
  // retb = trigger_actions(&pl);
  // if (retb == false) {
  //   log(LL_WARN, false, "main", "unable to trigger all actions");
  //   return false;
  // }

  // Report the event as a entry in the CSV output.
  report_event(&pl, cf);

  // Do not respond if the monologue mode is turned on.
  if (cf->cf_mono == true)
    return true;

  // Send a response back.
  retb = send_datagram(cts, sock, &pl, &addr, cf);
  if (retb == false) {
    log(LL_WARN, false, "main", "unable to send datagram on the %s socket", ipv);

    // Following the same logic as above in the receive stage.
    return !cf->cf_err;
  }

  return true;
}
