// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdbool.h>
#include <stdint.h>

#include "common/channel.h"
#include "common/plugin.h"
#include "common/log.h"
#include "common/payload.h"
#include "common/signal.h"
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
  struct channel ch;

  // Parse configuration from command-line options.
  retb = parse_config(&cf, argc, argv);
  if (retb == false) {
    log(LL_ERROR, false, "unable to parse the configuration");
    return EXIT_FAILURE;
  }

  // Verify that the compiled payload is exactly the expected size in bytes.
  if (sizeof(struct payload) != NEMO_PAYLOAD_SIZE) {
    log(LL_ERROR, false, "wrong payload size: expected %d, actual %zu",
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

  // Initialize the channel used to send and receive payloads.
  retb = open_channel(&ch, cf.cf_ipv4, (uint16_t)cf.cf_port, cf.cf_rbuf, cf.cf_sbuf, (uint8_t)cf.cf_ttl);
  if (retb == false) {
    log(LL_ERROR, false, "unable to create the %s channel", ch.ch_name);
    return EXIT_FAILURE;
  }

  // Start the main responding loop.
  retb = respond_loop(&ch, pi, npi, &cf);
  if (retb == false) {
    log(LL_ERROR, false, "responding loop has been terminated");
  }

  // Delete the socket.
  close_channel(&ch);

  // Terminate plugins.
  terminate_plugins(pi, npi);

  // Print final values of counters.
  log_channel(&ch);

  // Flush the standard output and error streams.
  retb = flush_report_stream(&cf);
  if (retb == false) {
    log(LL_ERROR, false, "unable to flush the report stream");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
