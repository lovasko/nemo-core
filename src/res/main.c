// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdbool.h>
#include <stdint.h>

#include "res/proto.h"
#include "res/types.h"
#include "util/daemon.h"
#include "util/log.h"


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
  struct counters cts4;
  struct counters cts6;

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

  // Initialise counters.
  if (opts.op_ipv4 == true) reset_counters(&cts4);
  if (opts.op_ipv6 == true) reset_counters(&cts6);

  // Start the main responding loop.
  retb = respond_loop(&cts4, &cts6, sock4, sock6, &opts);
  if (retb == false) {
    log(LL_ERROR, false, "main", "responding loop has finished");
    return EXIT_FAILURE;
  }

  // Terminate actions.
  free_plugins(pins, npins);

  // Print final values of counters.
  if (opts.op_ipv4 == true) log_counters("IPv4", &cts4);
  if (opts.op_ipv6 == true) log_counters("IPv6", &cts6);

  // Flush the standard output and error streams.
  retb = flush_report_stream(&opts);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to flush the report stream");
    return false;
  }

  return EXIT_SUCCESS;
}
