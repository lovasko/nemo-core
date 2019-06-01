// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdbool.h>
#include <stdint.h>

#include "common/plugin.h"
#include "common/log.h"
#include "common/payload.h"
#include "common/signal.h"
#include "common/socket.h"
#include "ures/funcs.h"
#include "ures/types.h"


/// Unicast network responder.
int
main(int argc, char* argv[])
{
  bool retb;
  struct config cf;
  struct plugin pi[PLUG_MAX];
  uint64_t npi;
  struct proto pr;

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

  // Install the signal handlers.
  retb = install_signal_handlers();
  if (retb == false) {
    log(LL_ERROR, false, "unable to install signal handlers");
    return EXIT_FAILURE;
  }

  // Start plugins.
  retb = load_plugins(pi, &npi, cf.cf_plgs);
  if (retb == false) {
    log(LL_ERROR, false, "unable to load all plugins");
    return EXIT_FAILURE;
  }

  // Start plugins.
  retb = start_plugins(pi, npi);
  if (retb == false) {
    log(LL_ERROR, false, "unable to start all plugins");
    return EXIT_FAILURE;
  }

  // Initialize the IPv4 or IPv6 connection.
  if (cf.cf_ipv4 == true) {
    reset_stats(&pr.pr_stat);
    pr.pr_name = "IPv4";

    retb = create_socket4(&pr, (uint16_t)cf.cf_port, cf.cf_rbuf, cf.cf_sbuf, (uint8_t)cf.cf_ttl);
    if (retb == false) {
      log(LL_ERROR, false, "unable to create %s socket", pr.pr_name);
      return EXIT_FAILURE;
    }
  } else {
    reset_stats(&pr.pr_stat);
    pr.pr_name = "IPv6";

    retb = create_socket6(&pr, (uint16_t)cf.cf_port, cf.cf_rbuf, cf.cf_sbuf, (uint8_t)cf.cf_ttl);
    if (retb == false) {
      log(LL_ERROR, false, "unable to create %s socket", pr.pr_name);
      return EXIT_FAILURE;
    }
  }

  // Start the main responding loop.
  retb = respond_loop(&pr, pi, npi, &cf);
  if (retb == false) {
    log(LL_ERROR, false, "responding loop has been terminated");
  }

  // Delete the socket.
  delete_socket(&pr);

  // Terminate plugins.
  terminate_plugins(pi, npi);

  // Print final values of counters.
  log_stats(pr.pr_name, &pr.pr_stat);

  // Flush the standard output and error streams.
  retb = flush_report_stream(&cf);
  if (retb == false) {
    log(LL_ERROR, false, "unable to flush the report stream");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
