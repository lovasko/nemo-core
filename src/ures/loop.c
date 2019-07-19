// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/select.h>

#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>

#include "common/channel.h"
#include "common/convert.h"
#include "common/log.h"
#include "common/now.h"
#include "common/signal.h"
#include "ures/funcs.h"
#include "ures/types.h"


/// Handle the incoming signal.
/// @return exit/continue decision
///
/// @global sint
/// @global sterm
/// @global susr1
/// @global schld
///
/// @param[in] ch  channel
/// @param[in] pi  array of plugins
/// @param[in] npi number of plugins
/// @param[in] cf  configuration
static bool
handle_interrupt(const struct channel* ch,
                 struct plugin* pi,
                 const uint64_t npi,
                 const struct config* cf)
{
  log(LL_TRACE, false, "handling interrupt");

  // Exit upon receiving SIGINT.
  if (sint == true) {
    log(LL_WARN, false, "received the %s signal", "SIGINT");
    return false;
  }

  // Exit upon receiving SIGTERM.
  if (sterm == true) {
    log(LL_WARN, false, "received the %s signal", "SIGTERM");
    return false;
  }

  // Check if any plugin processes changed state.
  if (schld == true) {
    log(LL_WARN, false, "received the %s signal", "SIGCHLD");
    wait_plugins(pi, npi);

    // Reset the signal indicator, so that following signal handling will
    // avoid the false positive.
    schld = false;
  }

  // Print logging information and continue the process upon receiving SIGUSR1.
  if (susr1 == true) {
    log_config(cf);
    log_plugins(pi, npi);
    log_channel(ch);

    // Reset the signal indicator, so that following signal handling will
    // avoid the false positive.
    susr1 = false;
    return true;
  }

  log(LL_WARN, false, "unknown interrupt occurred");
  return false;
}

/// Start responding to requests on a channel.
/// @return success/failure indication
///
/// @param[in] ch  channel
/// @param[in] pi  array of plugins
/// @param[in] npi number of plugins
/// @param[in] cf  configuration
bool
respond_loop(struct channel* ch,
             struct plugin* pi,
             const uint64_t npi,
             const struct config* cf)
{
  int reti;
  bool retb;
  fd_set rfd;
  sigset_t mask;
  struct timespec tout;
  struct timespec* ptout;
  uint64_t lim;
  uint64_t cur;
  char hn[NEMO_HOST_NAME_SIZE];
  int err;

  log(LL_INFO, false, "starting the response loop");
  log_config(cf);

  // Print the CSV header of the standard output.
  report_header(cf);

  // Obtain the host name.
  (void)memset(hn, 0, sizeof(hn));
  reti = gethostname(hn, sizeof(hn) - 1);
  if (reti == -1) {
    // Save the errno value so that it can be examined later.
    err = errno;

    log(LL_WARN, true, "unable to obtain host name");

    // Truncation of the host name is acceptable.
    if (err != ENAMETOOLONG) {
      return false;
    }
  }

  // Create the signal mask used for enabling signals during the pselect(2)
  // waiting.
  create_signal_mask(&mask);

  // Create the initial timeout.
  lim = mono_now() + cf->cf_ito;

  while (true) {
    // Compute the remaining time to wait for events.
    cur = mono_now();

    // Check whether the time is up.
    if (cf->cf_ito != 0 && cur >= lim) {
      break;
    }

    // Compute the timeout.
    if (cf->cf_ito == 0) {
      ptout = NULL;
    } else {
      fnanos(&tout, lim - cur);
      ptout = &tout;
    }

    log(LL_TRACE, false, "waiting for incoming datagrams");

    // Add the channel socket to the read event list.
    FD_ZERO(&rfd);
    FD_SET(ch->ch_sock, &rfd);

    // Wait for incoming datagram events. The number of file descriptors is based on the fact
    // that a standard process has three standard streams open, plus the socket, plus one.
    reti = pselect(4, &rfd, NULL, NULL, ptout, &mask);
    if (reti == -1) {
      // Check for interrupt (possibly due to a signal).
      if (errno == EINTR) {
        retb = handle_interrupt(ch, pi, npi, cf);
        if (retb == true) {
          continue;
        }

        return false;
      }

      log(LL_WARN, true, "waiting for events failed");
      return false;
    }

    // Stop the loop if the timeout was reached due to no incoming requests.
    if (reti == 0) {
      log(LL_WARN, false, "no incoming requests within time limit");
      return true;
    }

    // Handle incoming datagram.
    reti = FD_ISSET(ch->ch_sock, &rfd);
    if (reti > 0) {
      retb = handle_event(ch, hn, pi, npi, cf);
      if (retb == false) {
        return false;
      }

      // Replenish the inactivity timeout.
      lim = mono_now() + cf->cf_ito;
    }
  }

  return true;
}
