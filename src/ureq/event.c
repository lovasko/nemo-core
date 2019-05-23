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
#include "common/packet.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


/// Handle a network event by attempting to receive responses on all available sockets.
/// @return success/failure indication
///
/// @param[out] pr  protocol
/// @param[in]  rfd read file descriptors
/// @param[in]  cf  configuration
static bool
handle_event(struct proto* pr, const fd_set* rfd, const struct config* cf)
{
  int reti;
  bool retb;
  struct payload hpl;
  struct payload npl;
  struct sockaddr_storage addr;
  uint8_t ttl;

  reti = FD_ISSET(pr->pr_sock, rfd);
  if (reti > 0) {
    retb = receive_packet(pr, &addr, &hpl, &npl, &ttl, cf->cf_err);
    if (retb == false) {
      return false;
    }
  }

  // TODO report
  // TODO notify plugins

  return true;
}

/// Handle the incoming signal.
/// @return exit/continue decision
///
/// @global sint
/// @global sterm
/// @global susr1
///
/// @param[in] pr protocol
/// @param[in] cf configuration
static bool
handle_interrupt(const struct proto* pr, const struct config* cf)
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
    log_stats(pr->pr_name, &pr->pr_stat);
    log_socket_port(pr);

    // Reset the signal indicator, so that following signal handling will avoid
    // the false positive.
    susr1 = false;
    return true;
  }

  log(LL_WARN, false, "unknown interrupt occurred");
  return false;
}

/// Await and handle responses on both IPv4 and IPv6 sockets for a selected
/// duration of time.
/// @return success/failure indication
///
/// @global sint
/// @global sterm
/// @global susr1
///
/// @param[out] pr  protocol
/// @param[in]  dur duration to wait for responses
/// @param[in]  cf  configuration
bool
wait_for_events(struct proto* pr, const uint64_t dur, const struct config* cf)
{
  int reti;
  uint64_t cur;
  uint64_t goal;
  struct timespec todo;
  fd_set rfd;
  sigset_t mask;
  bool retb;

  // Ensure that all relevant events are registered.
  FD_ZERO(&rfd);
  FD_SET(pr->pr_sock, &rfd);

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

    // Start waiting on events. The number of file descriptors is based on the fact
    // that the process is expected to have three standard streams open, plus socket, plus one.
    reti = pselect(4, &rfd, NULL, NULL, &todo, &mask);
    if (reti == -1) {
      // Check for interrupt (possibly due to a signal).
      if (errno == EINTR) {
        retb = handle_interrupt(pr, cf);
        if (retb == true) {
          continue;
        }

        return false;
      }

      log(LL_WARN, true, "waiting for events failed");
      return false;
    }

    // Handle the network events by receiving and reporting responses.
    retb = handle_event(pr, &rfd, cf);
    if (retb == false) {
      return false;
    }

    // Update the current time.
    cur = mono_now();
  }

  return true;
}
