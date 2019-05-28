// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdbool.h>
#include <string.h>
#include <signal.h>

#include "common/signal.h"
#include "common/log.h"


volatile bool sint;  ///< SIGINT flag.
volatile bool sterm; ///< SIGTERM flag.
volatile bool susr1; ///< SIGUSR1 flag.
volatile bool shup;  ///< SIGHUP flag.
volatile bool schld; ///< SIGCHLD flag.

/// Signal handler for the SIGINT, SIGTERM, SIGUSR1, SIGHUP, and SIGCHLD signals.
///
/// This handler does not perform any action, just toggles the indicator
/// for the signal. The actual signal handling is done by the respond_loop
/// function.
///
/// @global sint
/// @global sterm
/// @global susr1
/// @global shup
/// @global schld
///
/// @param[in] sn signal number
static void
signal_handler(int sn)
{
  if (sn == SIGINT) {
    sint = true;
  }

  if (sn == SIGTERM) {
    sterm = true;
  }

  if (sn == SIGUSR1) {
    susr1 = true;
  }

  if (sn == SIGHUP) {
    shup = true;
  }

  if (sn == SIGCHLD) {
    schld = true;
  }
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

/// Install signal handlers for the SIGINT, SIGTERM, SIGUSR1, and SIGHUP
/// signals.
/// @return success/failure indication
///
/// @global sint
/// @global sterm
/// @global susr1
/// @global shup
/// @global schld
bool
install_signal_handlers(void)
{
  struct sigaction sa;
  int reti;
  int sigs[5] = {SIGINT, SIGTERM, SIGUSR1, SIGHUP, SIGCHLD};
  char* name;
  uint8_t i;

  log(LL_INFO, false, "installing signal handlers");

  // Reset the signal indicator.
  sint  = false;
  sterm = false;
  susr1 = false;
  shup  = false;
  schld = false;

  // This action makes sure that no system calls or execution context will get
  // interrupted by a signal. The pselect(2) call explicitly enables the
  // delivery of a set of signals, which in turn set the indicator flags.
  block_all_signals();

  // Initialise the handler settings.
  (void)memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;

  // Install signal handler for each signal.
  for (i = 0; i < 5; i++) {
    reti = sigaction(sigs[i], &sa, NULL);
    if (reti == -1) {
      name = strsignal(sigs[i]);
      log(LL_WARN, true, "unable to add signal handler for %s", name);
      return false;
    }
  }

  return true;
}

/// Create a signal mask that enables the SIGINT, SIGTERM, SIGUSR1, and SIGHUP
/// signals.
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
  (void)sigdelset(mask, SIGHUP);
  (void)sigdelset(mask, SIGCHLD);
}
