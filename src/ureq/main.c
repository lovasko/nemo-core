// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include "common/daemon.h"
#include "common/log.h"
#include "common/payload.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


/// Unicast network requester.
int
main(int argc, char* argv[])
{
  struct target tg[TARG_MAX];
  struct config cf;
  struct proto p4;
  struct proto p6;
  bool retb;

  // Parse command-line options.
  retb = parse_config(&cf, argc, argv);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to parse command-line options");
    return EXIT_FAILURE;
  }

  // Verify that the compiled payload is exactly the expected size in bytes.
  if (sizeof(struct payload) != NEMO_PAYLOAD_SIZE) {
    log(LL_ERROR, false, "main", "wrong payload size: expected %d, actual %z",
      NEMO_PAYLOAD_SIZE, sizeof(struct payload));
    return EXIT_FAILURE;
  }

  // Optionally turn the process into a daemon.
  if (cf.cf_dmon == true) {
    retb = turn_into_daemon();
    if (retb == false) {
      log(LL_ERROR, false, "main", "unable to turn process into daemon");
      return EXIT_FAILURE;
    }
  }

  // Install signal handlers.
  retb = install_signal_handlers();
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to install signal handlers");
    return EXIT_FAILURE;
  }

  // Create the IPv4 socket.
  retb = create_socket4(&p4, &cf);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to create the IPv4 socket");
    return EXIT_FAILURE;
  }

  // Create the IPv6 socket.
  retb = create_socket6(&p6, &cf);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to create the IPv6 socket");
    return EXIT_FAILURE;
  }

  // Start issuing requests and waiting for responses.
  retb = request_loop(&p4, &p6, tg, TARG_MAX, &cf);
  if (retb == false) {
    log(LL_ERROR, false, "main", "the request loop has ended unexpectedly");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
