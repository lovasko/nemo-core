// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/select.h>

#include <stdbool.h>
#include <errno.h>
#include <signal.h>

#include "common/log.h"
#include "ures/proto.h"
#include "ures/types.h"


extern volatile bool sterm;
extern volatile bool sint;
extern volatile bool susr1;

/// Handle the incoming signal.
/// @return exit/continue decision
///
/// @global sint
/// @global sterm
/// @global susr1
///
/// @param[in] cts4 IPv4 event counters
/// @param[in] cts6 IPv6 event counters
/// @param[in] cf   configuration
static bool
handle_interrupt(const struct counters* cts4,
                 const struct counters* cts6,
	         const struct config* cf)
{
  log(LL_TRACE, false, "main", "handling interrupt");

  // Exit upon receiving SIGINT.
  if (sint == true) {
    log(LL_WARN, false, "main", "received the %s signal", "SIGINT");
    return false;
  }

  // Exit upon receiving SIGTERM.
  if (sterm == true) {
    log(LL_WARN, false, "main", "received the %s signal", "SIGTERM");
    return false;
  }

  // Print logging information and continue the process upon receiving SIGUSR1.
  if (susr1 == true) {
    log_config(cf);
    if (cf->cf_ipv4 == true) log_counters("IPv4", cts4);
    if (cf->cf_ipv6 == true) log_counters("IPv6", cts6);

    // Reset the signal indicator, so that following signal handling will avoid
    // the false positive.
    susr1 = false;
    return true;
  }

  log(LL_WARN, false, "main", "unknown interrupt occurred");
  return false;
}

/// Start responding to requests on both IPv4 and IPv6 sockets.
/// @return success/failure indication
///
/// @global sint
/// @global sterm
/// @global susr1
///
/// @param[out] cts4  IPv4 event counters
/// @param[out] cts6  IPv6 event counters
/// @param[in]  sock4 IPv4 socket
/// @param[in]  sock6 IPv6 socket
/// @param[in]  cf    configuration
bool
respond_loop(struct counters* cts4,
             struct counters* cts6,
             int sock4,
             int sock6,
             const struct config* cf)
{
  int reti;
  int ndfs;
  bool retb;
  fd_set rfd;
  sigset_t mask;

  log(LL_INFO, false, "main", "starting the response loop");
  log_config(cf);

  // Print the CSV header of the standard output.
  report_header(cf);

  // Add sockets to the event list.
  FD_ZERO(&rfd);
  if (cf->cf_ipv4 == true) FD_SET(sock4, &rfd);
  if (cf->cf_ipv6 == true) FD_SET(sock6, &rfd);

  // Compute the file descriptor count.
  ndfs = (cf->cf_ipv4 == true && cf->cf_ipv6 == true) ? 5 : 4;

  // Create the signal mask used for enabling signals during the pselect(2)
  // waiting.
  create_signal_mask(&mask);

  while (true) {
    log(LL_TRACE, false, "main", "waiting for incoming datagrams");

    // Wait for incoming datagram events.
    reti = pselect(ndfs, &rfd, NULL, NULL, NULL, &mask);
    if (reti == -1) {
      // Check for interrupt (possibly due to a signal).
      if (errno == EINTR) {
        retb = handle_interrupt(cts4, cts6, cf);
        if (retb == true)
          continue;

        return false;
      }

      log(LL_WARN, true, "main", "waiting for events failed");
      return false;
    }

    // Handle incoming IPv4 datagrams.
    if (cf->cf_ipv4 == true) {
      reti = FD_ISSET(sock4, &rfd);
      if (reti > 0) {
        retb = handle_event(cts4, sock4, "IPv4", cf);
        if (retb == false)
          return false;
      }
    }

    // Handle incoming IPv6 datagrams.
    if (cf->cf_ipv6 == true) {
      reti = FD_ISSET(sock6, &rfd);
      if (reti > 0) {
        retb = handle_event(cts6, sock6, "IPv6", cf);
        if (retb == false)
          return false;
      }
    }
  }

  return true;
}
