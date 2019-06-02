// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdlib.h>

#include "common/channel.h"
#include "common/log.h"
#include "common/payload.h"
#include "common/signal.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


/// Unicast network requester.
int
main(int argc, char* argv[])
{
  struct target* tg;
  struct config cf;
  struct channel ch;
  bool retb;

  // Parse command-line options.
  retb = parse_config(&cf, argc, argv);
  if (retb == false) {
    log(LL_ERROR, false, "unable to parse command-line options");
    return EXIT_FAILURE;
  }

  // Verify that the compiled payload is exactly the expected size in bytes.
  if (sizeof(struct payload) != NEMO_PAYLOAD_SIZE) {
    log(LL_ERROR, false, "wrong payload size: expected %d, actual %z",
      NEMO_PAYLOAD_SIZE, sizeof(struct payload));
    return EXIT_FAILURE;
  }

  // Install signal handlers.
  retb = install_signal_handlers();
  if (retb == false) {
    log(LL_ERROR, false, "unable to install signal handlers");
    return EXIT_FAILURE;
  }

  // Initialize the IPv4 or IPv6 protocols.
  if (cf.cf_ipv4 == true) {
    retb = open_channel4(&ch, 0, cf.cf_rbuf, cf.cf_sbuf, (uint8_t)cf.cf_ttl);
    if (retb == false) {
      log(LL_ERROR, false, "unable to create the %s channel", ch.ch_name);
      return EXIT_FAILURE;
    }
  } else {
    retb = open_channel6(&ch, 0, cf.cf_rbuf, cf.cf_sbuf, (uint8_t)cf.cf_ttl);
    if (retb == false) {
      log(LL_ERROR, false, "unable to create %s socket", ch.ch_name);
      return EXIT_FAILURE;
    }
  }

  // Allocate the targets.
  tg = calloc((size_t)cf.cf_ntg, sizeof(*tg));
  if (tg == NULL) {
    log(LL_ERROR, true, "unable to allocate memory for targets");
    return false;
  }

  // Start issuing requests and waiting for responses.
  retb = request_loop(&ch, tg, &cf);
  if (retb == false) {
    log(LL_ERROR, false, "the request loop has terminated");
    return EXIT_FAILURE;
  }

  // Deallocate target arrays.
  free(tg);
  free(cf.cf_tg);

  // Close the channel.
  close_channel(&ch);

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
