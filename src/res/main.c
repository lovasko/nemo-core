// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>
#include <fcntl.h>
#include <dlfcn.h>

#include "common/payload.h"
#include "common/version.h"
#include "res/proto.h"
#include "res/types.h"
#include "util/convert.h"
#include "util/daemon.h"
#include "util/log.h"
#include "util/parse.h"
#include "util/ttl.h"


static volatile bool sint;  ///< Signal interrupt indicator.
static volatile bool sterm; ///< Signal termination indicator.

/// Signal handler for the SIGINT and SIGTERM signals.
///
/// This handler does not perform any action, just toggles the indicator
/// for the signal. The actual signal handling is done by the respond_loop
/// function.
///
/// @param[in] sig signal number
static void
signal_handler(int sig)
{
  if (sig == SIGINT)  sint  = true;
  if (sig == SIGTERM) sterm = true;
}

/// Install signal handler for the SIGINT signal.
/// @return success/failure indication
static bool
install_signal_handlers(void)
{
  struct sigaction sa;
  sigset_t ss;
  int reti;

  log(LL_INFO, false, "main", "installing signal handlers");

  // Reset the signal indicator.
  sint  = false;
  sterm = false;

  // Create and apply a set of blocked signals. We exclude the two recognised
  // signals from this set.
  (void)sigfillset(&ss);
  (void)sigdelset(&ss, SIGINT);
  (void)sigdelset(&ss, SIGTERM);
  (void)sigprocmask(SIG_SETMASK, &ss, NULL);

  // Initialise the handler settings.
  (void)memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;

  // Install signal handler for SIGINT.
  reti = sigaction(SIGINT, &sa, NULL);
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to add signal handler for %s",
        "SIGINT");
    return false;
  }

  // Install signal handler for SIGTERM.
  reti = sigaction(SIGTERM, &sa, NULL);
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to add signal handler for %s",
        "SIGTERM");
    return false;
  }

  return true;
}

/// Start responding to requests on both IPv4 and IPv6 sockets.
/// @return success/failure indication
///
/// @param[in] sock4 IPv4 socket
/// @param[in] sock6 IPv6 socket
/// @param[in] opts  command-line options
static bool
respond_loop(int sock4, int sock6, const struct options* opts)
{
  int reti;
  int ndfs;
  bool retb;
  fd_set rfd;

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

  while (true) {
    // Wait for incoming datagram events.
    reti = pselect(ndfs, &rfd, NULL, NULL, NULL, NULL);
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
    reti = FD_ISSET(sock4, &rfd);
    if (reti > 0) {
      retb = handle_event(sock4, "IPv4", opts);
      if (retb == false)
        return false;
    }

    // Handle incoming IPv6 datagrams.
    reti = FD_ISSET(sock6, &rfd);
    if (reti > 0) {
      retb = handle_event(sock6, "IPv6", opts);
      if (retb == false)
        return false;
    }
  }

  return true;
}

/// Unicast network responder.
int
main(int argc, char* argv[])
{
  bool retb;
  struct options opts;
  struct plugin pins[PLUG_MAX];
  uint64_t npins;
  int sock4;
  int sock6;

  // Parse command-line options.
  retb = parse_options(&opts, argc, argv);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to parse command-line options");
    return EXIT_FAILURE;
  }

  // Optionally turn the process into a daemon.
  if (opts.op_dmon == true) {
    retb = turn_into_daemon();
    if (retb == false) {
      log(LL_ERROR, false, "main", "unable to turn process into a daemon");
      return EXIT_FAILURE;
    }
  }

  // Install the signal handlers.
  retb = install_signal_handlers();
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to install signal handlers");
    return EXIT_FAILURE;
  }

  // Create the IPv4 socket.
  retb = create_socket4(&sock4, &opts);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to create %s socket", "IPv4");
    return EXIT_FAILURE;
  }

  // Create the IPv6 socket.
  retb = create_socket6(&sock6, &opts);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to create %s socket", "IPv6");
    return EXIT_FAILURE;
  }

  // Start actions.
  retb = load_plugins(pins, &npins, &opts);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to load all actions");
    return EXIT_FAILURE;
  }

  // Start actions.
  retb = init_plugins(pins, npins);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to start all actions");
    return EXIT_FAILURE;
  }

  // Start the main responding loop.
  retb = respond_loop(sock4, sock6, &opts);
  if (retb == false) {
    log(LL_ERROR, false, "main", "responding loop has finished");
    return EXIT_FAILURE;
  }

  // Terminate actions.
  free_plugins(pins, npins);

  // Flush the standard output and error streams.
  retb = flush_report_stream(&opts);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to flush the report stream");
    return false;
  }

  return EXIT_SUCCESS;
}
