// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdbool.h>
#include <stdint.h>

#include "common/daemon.h"
#include "common/log.h"
#include "common/payload.h"
#include "ures/funcs.h"
#include "ures/types.h"


/// Unicast network responder.
int
main(int argc, char* argv[])
{
  bool retb;
  struct config cf;
  struct plugin pins[PLUG_MAX];
  uint64_t npins;
  struct proto p4;
  struct proto p6;

  // Parse configuration from command-line options.
  retb = parse_config(&cf, argc, argv);
  if (retb == false) {
    log(LL_ERROR, false, "unable to parse the configuration");
    return EXIT_FAILURE;
  }

  // Verify that the compiled payload is exactly the expected size in bytes.
  if (sizeof(struct payload) != NEMO_PAYLOAD_SIZE) {
    log(LL_ERROR, false, "wrong payload size: expected %d, actual %z",
      NEMO_PAYLOAD_SIZE, sizeof(struct payload));
    return EXIT_FAILURE;
  }

  // Optionally turn the process into a daemon.
  if (cf.cf_dmon == true) {
    retb = turn_into_daemon();
    if (retb == false) {
      log(LL_ERROR, false, "unable to turn process into a daemon");
      return EXIT_FAILURE;
    }
  }

  // Install the signal handlers.
  retb = install_signal_handlers();
  if (retb == false) {
    log(LL_ERROR, false, "unable to install signal handlers");
    return EXIT_FAILURE;
  }

  // Start plugins.
  retb = load_plugins(pins, &npins, &cf);
  if (retb == false) {
    log(LL_ERROR, false, "unable to load all plugins");
    return EXIT_FAILURE;
  }

  // Start plugins.
  retb = start_plugins(pins, npins);
  if (retb == false) {
    log(LL_ERROR, false, "unable to start all plugins");
    return EXIT_FAILURE;
  }

  // Initialize the IPv4 connection.
  if (cf.cf_ipv4 == true) {
    reset_counters(&p4);
    p4.pr_name = "IPv4";

    retb = create_socket4(&p4, &cf);
    if (retb == false) {
      log(LL_ERROR, false, "unable to create %s socket", p4.pr_name);
      return EXIT_FAILURE;
    }
  }

  // Initialize the IPv6 connection.
  if (cf.cf_ipv6 == true) {
    reset_counters(&p6);
    p6.pr_name = "IPv6";

    retb = create_socket6(&p6, &cf);
    if (retb == false) {
      log(LL_ERROR, false, "unable to create %s socket", p6.pr_name);
      return EXIT_FAILURE;
    }
  }

  // Start the main responding loop.
  retb = respond_loop(&p4, &p6, pins, npins, &cf);
  if (retb == false)
    log(LL_ERROR, false, "responding loop has been terminated");

  if (cf.cf_ipv4 == true) delete_socket(&p4);
  if (cf.cf_ipv6 == true) delete_socket(&p6);

  // Terminate plugins.
  terminate_plugins(pins, npins);

  // Print final values of counters.
  if (cf.cf_ipv4 == true) log_counters(&p4);
  if (cf.cf_ipv6 == true) log_counters(&p6);

  // Flush the standard output and error streams.
  retb = flush_report_stream(&cf);
  if (retb == false) {
    log(LL_ERROR, false, "unable to flush the report stream");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
