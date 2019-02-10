// Copyright (c) 2018-2019 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/select.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>

#include "common/log.h"
#include "common/convert.h"
#include "common/now.h"
#include "common/signal.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


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
handle_interrupt(const struct proto* p4,
                 const struct proto* p6,
                 const struct config* cf)
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
bool
wait_for_events(struct proto* p4,
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
