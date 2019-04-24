// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/select.h>

#include <stdbool.h>
#include <errno.h>
#include <signal.h>

#include "common/log.h"
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
      log_stats(p4->pr_name, &p4->pr_stat);
    }

    if (cf->cf_ipv6 == true) {
      log_stats(p6->pr_name, &p6->pr_stat);
    }

    // Reset the signal indicator, so that following signal handling will avoid
    // the false positive.
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
/// @param[out] p4    IPv4 protocol
/// @param[out] p6    IPv6 protocol
/// @param[in]  pins  array of plugins
/// @param[in]  npins number of plugins
/// @param[in]  cf    configuration
bool
respond_loop(struct proto* p4,
             struct proto* p6,
             const struct plugin* pins,
             const uint64_t npins,
             const struct config* cf)
{
  int reti;
  int nfds;
  bool retb;
  fd_set rfd;
  sigset_t mask;

  log(LL_INFO, false, "starting the response loop");
  log_config(cf);

  // Print the CSV header of the standard output.
  report_header(cf);

  // Add sockets to the event list.
  FD_ZERO(&rfd);
  nfds = 3; //< Standard input, output, and error streams; plus one.

  if (cf->cf_ipv4 == true) {
    FD_SET(p4->pr_sock, &rfd);
    nfds++;
  }

  if (cf->cf_ipv6 == true) {
    FD_SET(p6->pr_sock, &rfd);
    nfds++;
  }

  // Create the signal mask used for enabling signals during the pselect(2)
  // waiting.
  create_signal_mask(&mask);

  while (true) {
    log(LL_TRACE, false, "waiting for incoming datagrams");

    // Wait for incoming datagram events.
    reti = pselect(nfds, &rfd, NULL, NULL, NULL, &mask);
    if (reti == -1) {
      // Check for interrupt (possibly due to a signal).
      if (errno == EINTR) {
        retb = handle_interrupt(p4, p6, cf);
        if (retb == true) {
          continue;
        }

        return false;
      }

      log(LL_WARN, true, "waiting for events failed");
      return false;
    }

    // Handle incoming IPv4 datagrams.
    if (cf->cf_ipv4 == true) {
      reti = FD_ISSET(p4->pr_sock, &rfd);
      if (reti > 0) {
        retb = handle_event(p4, pins, npins, cf);
        if (retb == false) {
          return false;
        }
      }
    }

    // Handle incoming IPv6 datagrams.
    if (cf->cf_ipv6 == true) {
      reti = FD_ISSET(p6->pr_sock, &rfd);
      if (reti > 0) {
        retb = handle_event(p6, pins, npins, cf);
        if (retb == false) {
          return false;
        }
      }
    }
  }

  return true;
}
