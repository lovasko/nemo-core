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
    log(LL_ERROR, false, "unable to parse command-line options");
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
      log(LL_ERROR, false, "unable to turn process into daemon");
      return EXIT_FAILURE;
    }
  }

  // Install signal handlers.
  retb = install_signal_handlers();
  if (retb == false) {
    log(LL_ERROR, false, "unable to install signal handlers");
    return EXIT_FAILURE;
  }

  // Initialize the IPv4 connection.
  if (cf.cf_ipv4 == true) {
    reset_counters(&p4);
    p4.pr_name = "IPv4";
    p4.pr_ipv  = 4;

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
    p6.pr_ipv  = 6;

    retb = create_socket6(&p6, &cf);
    if (retb == false) {
      log(LL_ERROR, false, "unable to create %s socket", p6.pr_name);
      return EXIT_FAILURE;
    }
  }

  // Start issuing requests and waiting for responses.
  retb = request_loop(&p4, &p6, tg, TARG_MAX, &cf);
  if (retb == false) {
    log(LL_ERROR, false, "the request loop has ended unexpectedly");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
