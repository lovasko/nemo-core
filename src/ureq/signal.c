// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdbool.h>
#include <signal.h>
#include <string.h>

#include "common/log.h"


volatile bool sint;  ///< SIGINT flag.
volatile bool sterm; ///< SIGTERM flag.
volatile bool susr1; ///< SIGUSR1 flag.

/// Signal handler for the SIGINT and SIGTERM signals.
///
/// This handler does not perform any action, just toggles the indicator
/// for the signal. The actual signal handling is done by the respond_loop
/// function.
///
/// @global sint
/// @global sterm
/// @global susr1
///
/// @param[in] sig signal number
static void
signal_handler(int sig)
{
  if (sig == SIGINT)  { sint  = true; }
  if (sig == SIGTERM) { sterm = true; }
  if (sig == SIGUSR1) { susr1 = true; }
}

/// Block the delivery of all signals, except the two signals that can not be
/// intercepted.
static void
block_all_signals(void)
{
  sigset_t mask;

  // All signals that are not part of the set are standard signals and
  // therefore the listed errors do not apply.
  (void)sigfillset(&mask);
  (void)sigdelset(&mask, SIGSTOP);
  (void)sigdelset(&mask, SIGKILL);

  // None of the two errno values apply to the call and therefore we can assume
  // that the call always succeeds.
  (void)sigprocmask(SIG_SETMASK, &mask, NULL);
}

/// Install signal handlers for the SIGINT and SIGTER signals.
/// @return success/failure indication
///
/// @global sint
/// @global sterm
/// @global susr1
bool
install_signal_handlers(void)
{
  struct sigaction sa;
  int reti;

  log(LL_INFO, false, "main", "installing signal handlers");

  // Reset the signal indicator.
  sint  = false;
  sterm = false;

  // This action makes sure that no system calls or execution context will get
  // interrupted by a signal. The pselect(2) call explicitly enables the
  // delivery of a set of signals, which in turn set the indicator flags.
  block_all_signals();

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

  // Install signal handler for SIGUSR1.
  reti = sigaction(SIGUSR1, &sa, NULL);
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to add signal handler for %s",
        "SIGUSR1");
    return false;
  }

  return true;
}

/// Create a signal mask that enables the SIGINT, SIGTERM, and SIGUSR1 signals.
///
/// @param[out] mask signal mask applied while waiting
void
create_signal_mask(sigset_t* mask)
{
  (void)sigfillset(mask);
  (void)sigdelset(mask, SIGSTOP);
  (void)sigdelset(mask, SIGKILL);
  (void)sigdelset(mask, SIGINT);
  (void)sigdelset(mask, SIGTERM);
  (void)sigdelset(mask, SIGUSR1);
}
