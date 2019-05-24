// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/select.h>

#include <stdbool.h>
#include <errno.h>
#include <signal.h>

#include "common/convert.h"
#include "common/log.h"
#include "common/now.h"
#include "common/signal.h"
#include "ures/funcs.h"
#include "ures/types.h"


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
handle_interrupt(const struct proto* pr,
                 const struct plugin* pi,
                 const uint64_t npi,
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
    log_plugins(pi, npi);
    log_stats(pr->pr_name, &pr->pr_stat);

    // Reset the signal indicator, so that following signal handling will
    // avoid the false positive.
    susr1 = false;
    return true;
  }

  log(LL_WARN, false, "unknown interrupt occurred");
  return false;
}

/// Start responding to requests on both IPv4 and IPv6 sockets.
/// @return success/failure indication
///
/// @global sint
/// @global sterm
/// @global susr1
///
/// @param[out] pr    protocol
/// @param[in]  pins  array of plugins
/// @param[in]  npins number of plugins
/// @param[in]  cf    configuration
bool
respond_loop(struct proto* pr,
             const struct plugin* pins,
             const uint64_t npins,
             const struct config* cf)
{
  int reti;
  bool retb;
  fd_set rfd;
  sigset_t mask;
  struct timespec tout;
  struct timespec* ptout;
  uint64_t lim;
  uint64_t cur;

  log(LL_INFO, false, "starting the response loop");
  log_config(cf);

  // Print the CSV header of the standard output.
  report_header(cf);

  // Add the protocol socket to the read event list.
  FD_ZERO(&rfd);
  FD_SET(pr->pr_sock, &rfd);

  // Create the signal mask used for enabling signals during the pselect(2)
  // waiting.
  create_signal_mask(&mask);

  // Create the initial timeout.
  lim = mono_now() + cf->cf_ito;

  while (true) {
    // Compute the remaining time to wait for events.
    cur = mono_now();

    // Check whether the time is up.
    if (cf->cf_ito != 0 && cur >= lim) {
      break;
    }

    // Compute the timeout.
    if (cf->cf_ito == 0) {
      ptout = NULL;
    } else {
      fnanos(&tout, lim - cur);
      ptout = &tout;
    }

    log(LL_TRACE, false, "waiting for incoming datagrams");

    // Wait for incoming datagram events. The number of file descriptors is based on the fact
    // that a standard process has three standard streams open, plus the socket, plus one.
    reti = pselect(4, &rfd, NULL, NULL, ptout, &mask);
    if (reti == -1) {
      // Check for interrupt (possibly due to a signal).
      if (errno == EINTR) {
        retb = handle_interrupt(pr, pins, npins, cf);
        if (retb == true) {
          continue;
        }

        return false;
      }

      log(LL_WARN, true, "waiting for events failed");
      return false;
    }

    // Stop the loop if the timeout was reached due to no incoming requests.
    if (reti == 0) {
      log(LL_WARN, false, "no incoming requests within time limit");
      return true;
    }

    // Handle incoming IPv4 datagrams.
    reti = FD_ISSET(pr->pr_sock, &rfd);
    if (reti > 0) {
      retb = handle_event(pr, pins, npins, cf);
      if (retb == false) {
        return false;
      }

      // Replenish the inactivity timeout.
      lim = mono_now() + cf->cf_ito;
    }
  }

  return true;
}
