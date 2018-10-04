// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/select.h>

#include <stdbool.h>
#include <errno.h>
#include <signal.h>

#include "res/proto.h"
#include "res/types.h"
#include "util/log.h"


extern volatile bool sterm;
extern volatile bool sint;

/// Start responding to requests on both IPv4 and IPv6 sockets.
/// @return success/failure indication
///
/// @global sint
/// @global sterm
///
/// @param[in] sock4 IPv4 socket
/// @param[in] sock6 IPv6 socket
/// @param[in] opts  command-line options
bool
respond_loop(int sock4, int sock6, const struct options* opts)
{
  int reti;
  int ndfs;
  bool retb;
  fd_set rfd;
  sigset_t mask;

  log(LL_INFO, false, "main", "starting the response loop");
  log_options(opts);

  // Print the CSV header of the standard output.
  report_header(opts);

  // Add sockets to the event list.
  FD_ZERO(&rfd);
  if (opts->op_ipv4 == true) FD_SET(sock4, &rfd);
  if (opts->op_ipv6 == true) FD_SET(sock6, &rfd);

  // Compute the file descriptor count.
  ndfs = (opts->op_ipv4 == true && opts->op_ipv6 == true) ? 5 : 4;

  // Create the signal mask used for enabling signals during the pselect(2)
  // waiting.
  create_signal_mask(&mask);

  while (true) {
    // Wait for incoming datagram events.
    reti = pselect(ndfs, &rfd, NULL, NULL, NULL, &mask);
    if (reti == -1) {
      // Check for interrupt (possibly due to a signal).
      if (errno == EINTR) {
        if (sint == true)
          log(LL_WARN, false, "main", "received the %s signal", "SIGINT");
        else if (sterm == true)
          log(LL_WARN, false, "main", "received the %s signal", "SIGTERM");
        else
          log(LL_WARN, false, "main", "unknown event queue interrupt");

        return false;
      }

      log(LL_WARN, true, "main", "waiting for events failed");
      return false;
    }

    // Handle incoming IPv4 datagrams.
    if (opts->op_ipv4 == true) {
      reti = FD_ISSET(sock4, &rfd);
      if (reti > 0) {
        retb = handle_event(sock4, "IPv4", opts);
        if (retb == false)
          return false;
      }
    }

    // Handle incoming IPv6 datagrams.
    if (opts->op_ipv6 == true) {
      reti = FD_ISSET(sock6, &rfd);
      if (reti > 0) {
        retb = handle_event(sock6, "IPv6", opts);
        if (retb == false)
          return false;
      }
    }
  }

  return true;
}
