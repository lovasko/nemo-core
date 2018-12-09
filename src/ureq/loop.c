// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/select.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <inttypes.h>

#include "common/convert.h"
#include "common/log.h"
#include "common/now.h"
#include "common/payload.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


extern volatile bool sterm;
extern volatile bool sint;
extern volatile bool susr1;

/// Fill the payload with default and selected data.
///
/// @param[out] pl   payload
/// @param[in]  tg   network target
/// @param[in]  snum sequence number
/// @param[in]  cf   configuration
static void
fill_payload(struct payload *pl,
             const struct target* tg,
             const uint64_t snum,
             const struct proto* pr,
             const struct config* cf)
{
  (void)memset(pl, 0, sizeof(*pl));
  pl->pl_mgic  = NEMO_PAYLOAD_MAGIC;
  pl->pl_fver  = NEMO_PAYLOAD_VERSION;
  pl->pl_type  = NEMO_PAYLOAD_TYPE_REQUEST;
  pl->pl_port  = (uint16_t)cf->cf_port;
  pl->pl_ttl1  = (uint8_t)cf->cf_ttl;
  pl->pl_pver  = pr->pr_ipv;
  pl->pl_snum  = snum;
  pl->pl_slen  = cf->cf_cnt;
  pl->pl_reqk  = cf->cf_key;
  pl->pl_laddr = tg->tg_laddr;
  pl->pl_haddr = tg->tg_haddr;
  pl->pl_rtm1  = real_now();
  pl->pl_mtm1  = mono_now();
}

/// Convert the target address to a universal standard address type.
///
/// @param[out] ss universal address type
/// @param[in]  tg network target
static void
set_address(struct sockaddr_storage* ss, const struct target* tg, const struct config* cf)
{
  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;

  if (tg->tg_ipv == NEMO_IP_VERSION_4) {
    (void)memset(&sin, 0, sizeof(sin));
    sin.sin_family      = AF_INET;
    sin.sin_port        = htons((uint16_t)cf->cf_port);
    sin.sin_addr.s_addr = (uint32_t)tg->tg_laddr;
    (void)memcpy(ss, &sin, sizeof(sin));
  }

  if (tg->tg_ipv == NEMO_IP_VERSION_6) {
    (void)memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port   = htons((uint16_t)cf->cf_port);
    tipv6(&sin6.sin6_addr, tg->tg_laddr, tg->tg_haddr);
    (void)memcpy(ss, &sin6, sizeof(sin6));
  }
}

/// Send a request to a network target.
/// @return success/failure indication
///
/// @param[in] pr protocol connection
/// @param[in] tg network target
/// @param[in] cf configuration
static bool
send_request(struct proto* pr,
             const uint64_t snum,
             const struct target* tg,
             const struct config* cf)
{
  struct payload pl;
  struct msghdr msg;
  struct iovec data;
  struct sockaddr_storage ss;
  ssize_t n;
  uint8_t lvl;

  log(LL_TRACE, false, "sending a request");

  // Prepare payload data.
  data.iov_base = &pl;
  data.iov_len  = sizeof(pl);

  // Prepare the message.
  set_address(&ss, tg, cf);
  msg.msg_name       = &ss;
  msg.msg_namelen    = sizeof(ss);
  msg.msg_iov        = &data;
  msg.msg_iovlen     = 1;
  msg.msg_control    = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags      = 0;

  // Prepare the payload content.
  fill_payload(&pl, tg, snum, pr, cf);
  encode_payload(&pl);

  // Send the datagram.
  n = sendmsg(pr->pr_sock, &msg, MSG_DONTWAIT);
  if (n == -1) {
    // Increase the seriousness of the incident in case we are going to fail.
    lvl = cf->cf_err == true ? LL_WARN : LL_DEBUG;
    log(lvl, true, "unable to send datagram");

    pr->pr_seni++;

    // Fail the procedure only if we have selected the exit-on-error attribute
    // to be true.
    return !cf->cf_err;
  }

  // Verify the number of sent bytes.
  if (n != (ssize_t)sizeof(pl)) {
    // Increase the seriousness of the incident in case we are going to fail.
    lvl = cf->cf_err == true ? LL_WARN : LL_DEBUG;
    log(lvl, true, "unable to send the whole payload");

    pr->pr_seni++;

    // Fail the procedure only if we have selected the exit-on-error attribute
    // to be true.
    return !cf->cf_err;
  }

  pr->pr_sall++;

  return true;
}

/// Handle the incoming signal.
/// @return exit/continue decision
///
/// @global sint
/// @global sterm
/// @global susr1
///
/// @param[in] p4 IPv4 protocol
/// @param[in] p6 IPv6 protocol
/// @param[in] cf configuration
static bool
handle_interrupt(const struct proto* p4, const struct proto* p6, const struct config* cf)
{
  log(LL_TRACE, false, "handling interrupt");

  // Exit upon receiving SIGINT.
  if (sint == true) {
    log(LL_WARN, false, "received the %s signal", "SIGINT");
    return false;
  }

  // Exit upon receiving SIGTERM.
  if (sterm == true) {
    log(LL_WARN, false, "received the %s signal", "SIGTERM");
    return false;
  }

  // Print logging information and continue the process upon receiving SIGUSR1.
  if (susr1 == true) {
    log_config(cf);
    if (cf->cf_ipv4 == true) {
      log_counters(p4);
      log_socket_port(p4);
    }

    if (cf->cf_ipv6 == true) {
      log_counters(p6);
      log_socket_port(p6);
    }

    // Reset the signal indicator, so that following signal handling will avoid
    // the false positive.
    susr1 = false;
    return true;
  }

  log(LL_WARN, false, "unknown interrupt occurred");
  return false;
}

/// Receive a payload from the network.
/// @return success/failure indication
///
/// @param[out] pl payload
/// @param[out] pr protocol connection
/// @param[in]  cf configuration
static bool
receive_response(struct payload* pl, struct proto* pr, const struct config* cf)
{
  struct msghdr msg;
  ssize_t n;
  bool retb;
  struct iovec data;
  struct sockaddr_storage addr;

  // Prepare payload data.
  data.iov_base = pl;
  data.iov_len  = sizeof(*pl);

  // Prepare the message.
  msg.msg_name    = &addr;
  msg.msg_namelen = sizeof(addr);
  msg.msg_iov     = &data;
  msg.msg_iovlen  = 1;
  msg.msg_flags   = 0;

  // Receive the message and handle potential errors.
  n = recvmsg(pr->pr_sock, &msg, MSG_DONTWAIT | MSG_TRUNC);
  if (n < 0) {
    log(LL_WARN, true, "receiving has failed");
    pr->pr_reni++;

    if (cf->cf_err == true)
      return false;
  }

  // Convert the payload from its on-wire format.
  decode_payload(pl);

  // Verify the payload correctness.
  retb = verify_payload(pr, n, pl);
  if (retb == false) {
    log(LL_WARN, false, "invalid payload content");
    return false;
  }

  return true;
}

/// Handle a network event by attempting to receive responses on all available sockets.
/// @return success/failure indication
///
/// @param[out] p4  IPv4 connection
/// @param[out] p6  IPv6 connection
/// @param[in]  rfd read file descriptors
/// @param[in]  cf  configuration
static bool
handle_event(struct proto* p4, struct proto* p6, const fd_set* rfd, const struct config* cf)
{
  int reti;
  bool retb;
  struct payload pl;

  // Handle incoming response on the IPv4 socket.
  if (cf->cf_ipv4 == true) {
    reti = FD_ISSET(p4->pr_sock, rfd);
    if (reti > 0) {
      retb = receive_response(&pl, p4, cf);
      if (retb == false)
        return false;
    }
  }

  // Handle incoming response on the IPv6 socket.
  if (cf->cf_ipv6 == true) {
    reti = FD_ISSET(p6->pr_sock, rfd);
    if (reti > 0) {
      retb = receive_response(&pl, p6, cf);
      if (retb == false)
        return false;
    }
  }

  return true;
}

/// Prepare file set used in the pselect(2) function.
///
/// @param[out] rfd  read file descriptors
/// @param[out] nfds number of file descriptors
/// @param[in]  p4   IPv4 protocol connection
/// @param[in]  p6   IPv6 protocol connection
/// @param[in]  cf   configuration
static void
prepare_file_set(fd_set* rfd,
                 int* nfds,
                 const struct proto* p4,
                 const struct proto* p6,
                 const struct config* cf)
{
  // Add sockets to the event list.
  FD_ZERO(rfd);
  if (cf->cf_ipv4 == true)
    FD_SET(p4->pr_sock, rfd);
  if (cf->cf_ipv6 == true)
    FD_SET(p6->pr_sock, rfd);

  // Compute the file descriptor count.
  *nfds = (cf->cf_ipv4 == true && cf->cf_ipv6 == true) ? 5 : 4;
}

/// Await and handle responses on both IPv4 and IPv6 sockets for a selected
/// duration of time.
/// @return success/failure indication
///
/// @global sint
/// @global sterm
/// @global susr1
///
/// @param[out] p4  IPv4 protocol connection
/// @param[out] p6  IPv6 protocol connection
/// @param[in]  dur duration to wait for responses
/// @param[in]  cf  configuration
static bool
wait_for_responses(struct proto* p4,
                   struct proto* p6,
                   const uint64_t dur,
                   const struct config* cf)
{
  int reti;
  uint64_t cur;
  uint64_t goal;
  struct timespec todo;
  fd_set rfd;
  int nfds;
  sigset_t mask;
  bool retb;

  // Ensure that all relevant events are registered.
  prepare_file_set(&rfd, &nfds, p4, p6, cf);

  // Create the signal mask used for enabling signals during the pselect(2) waiting.
  create_signal_mask(&mask);

  // Set the goal time to be in the future.
  cur  = mono_now();
  goal = cur + dur;

  // Repeat the waiting process until the sufficient time has passed.
  while (cur < goal) {
    log(LL_TRACE, false, "waiting for responses");

    // Compute the time left to wait for the responses.
    fnanos(&todo, goal - cur);

    // Start waiting on events.
    reti = pselect(nfds, &rfd, NULL, NULL, &todo, &mask);
    if (reti == -1) {
      // Check for interrupt (possibly due to a signal).
      if (errno == EINTR) {
        retb = handle_interrupt(p4, p6, cf);
        if (retb == true)
          continue;

        return false;
      }

      log(LL_WARN, true, "waiting for events failed");
      return false;
    }

    // Handle the network events by receiving and reporting responses.
    retb = handle_event(p4, p6, &rfd, cf);
    if (retb == false)
      return false;

    // Update the current time.
    cur = mono_now();
  }

  return true;
}

/// Issue a request against a target.
/// @return success/failure indication
///
/// @param[out] p4   IPv4 protocol connection
/// @param[out] p6   IPv6 protocol connection
/// @param[in]  snum sequence number
/// @param[in]  tg   network target
/// @param[in]  cf   configuration
static bool
contact_target(struct proto* p4,
               struct proto* p6,
               const uint64_t snum,
               const struct target* tg,
               const struct config* cf)
{
  bool retb;

  // Handle the IPv4 case.
  if (cf->cf_ipv4 == true && tg->tg_ipv == NEMO_IP_VERSION_4) { 
    retb = send_request(p4, snum, tg, cf);
    if (retb == false) {
      log(LL_WARN, false, "unable to send a request");
      return false;
    }
  }

  // Handle the IPv6 case.
  if (cf->cf_ipv6 == true && tg->tg_ipv == NEMO_IP_VERSION_6) { 
    retb = send_request(p6, snum, tg, cf);
    if (retb == false) {
      log(LL_WARN, false, "unable to send a request");
      return false;
    }
  }

  return true;
}

/// Single round of issued requests with small pauses after each request.
/// @return success/failure indication
///
/// @param[out] p4  IPv4 protocol connection
/// @param[out] p6  IPv6 protocol connection
/// @param[in]  tg  array of network targets
/// @param[in]  ntg number of network targets
/// @param[in]  sn  sequence number
/// @param[in]  cf  configuration
static bool
dispersed_round(struct proto* p4,
                struct proto* p6,
                const struct target* tg,
                const uint64_t ntg,
                const uint64_t snum,
                const struct config* cf)
{
  uint64_t i;
  uint64_t part;
  bool retb;

  // In case there are no targets, just sleep throughout the whole round.
  if (ntg == 0) {
    retb = wait_for_responses(p4, p6, cf->cf_int, cf);
    if (retb == false) {
      log(LL_WARN, false, "unable to wait for responses");
      return false;
    }
  }

  // Compute the time to sleep between each request in the round. We can safely
  // divide by the number of targets, as we have previously handled the case of
  // no targets.
  part = (cf->cf_int / ntg) + 1;

  // Issue all requests.
  for (i = 0; i < ntg; i++) {
    retb = contact_target(p4, p6, snum, &tg[i], cf);
    if (retb == false)
      return false;

    // Await responses the divided part of the round.
    retb = wait_for_responses(p4, p6, part, cf);
    if (retb == false) {
      log(LL_WARN, false, "unable to wait for responses");
      return false;
    }
  }

  return true;
}

/// Single round of issued requests with no pauses after each request, followed
/// by a single full pause.
/// @return success/failure indication
///
/// @param[out] p4  IPv4 protocol connection
/// @param[out] p6  IPv6 protocol connection
/// @param[in]  tg  array of network targets
/// @param[in]  ntg number of network targets
/// @param[in]  sn  sequence number
/// @param[in]  cf  configuration
static bool
grouped_round(struct proto* p4,
              struct proto* p6,
              const struct target* tg,
              const uint64_t ntg,
              const uint64_t snum,
              const struct config* cf)
{
  uint64_t i;
  bool retb;

  // Issue all requests.
  for (i = 0; i < ntg; i++) {
    retb = contact_target(p4, p6, snum, &tg[i], cf);
    if (retb == false)
      return false;
  }

  // Await responses for the remainder of the interval.
  retb = wait_for_responses(p4, p6, cf->cf_int, cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to wait for responses");
    return false;
  }

  return true;
}

/// Main request loop.
/// @return success/failure indication
///
/// @param[in] p4 IPv4 protocol connection
/// @param[in] p6 IPv6 protocol connection
/// @param[in] cf configuration
bool
request_loop(struct proto* p4,
             struct proto* p6,
             struct target* tg,
             const uint64_t tmax,
             const struct config* cf)
{
  uint64_t i;
  uint64_t ntg;
  uint64_t rld;
  uint64_t now;
  bool retb;

  // Log the current configuration.
  log_config(cf);

  // Print the CSV header of the standard output.
  report_header(cf);

  // Load all targets at start.
  retb = load_targets(tg, &ntg, tmax, cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to load targets");
    return false;
  }

  // Set the next reload time to be in the future.
  now = mono_now();
  rld = now + cf->cf_rld;

  for (i = 0; i < cf->cf_cnt; i++) {
    log(LL_TRACE, false, "request round %" PRIu64 "/%" PRIu64, i + 1, cf->cf_cnt);

    // Check if name resolution needs to happen.
    now = mono_now();
    if (now > rld) {
      retb = load_targets(tg, &ntg, tmax, cf);
      if (retb == false) {
        log(LL_WARN, false, "unable to load targets");
        return false;
      }

      // Update the next refresh time.
      rld = now + cf->cf_rld;
    }

    if (cf->cf_grp == true) {
      retb = grouped_round(p4, p6, tg, ntg, i, cf);
      if (retb == false)
        return false;
    } else {
      retb = dispersed_round(p4, p6, tg, ntg, i, cf);
      if (retb == false)
        return false;
    }
  }

  // Waiting for incoming responses after sending all requests.
  log(LL_TRACE, false, "waiting for final responses");
  retb = wait_for_responses(p4, p6, cf->cf_wait, cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to wait for final responses");
    return false;
  }

  return true;
}
