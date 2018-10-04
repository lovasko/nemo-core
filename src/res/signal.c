// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdbool.h>
#include <signal.h>
#include <string.h>

#include "res/proto.h"
#include "res/types.h"
#include "util/log.h"


volatile bool sint;  ///< Signal interrupt indicator.
volatile bool sterm; ///< Signal termination indicator.

/// Signal handler for the SIGINT and SIGTERM signals.
///
/// This handler does not perform any action, just toggles the indicator
/// for the signal. The actual signal handling is done by the respond_loop
/// function.
///
/// @global sint
/// @global sterm
///
/// @param[in] sig signal number
static void
signal_handler(int sig)
{
  if (sig == SIGINT)  sint  = true;
  if (sig == SIGTERM) sterm = true;
}

/// Install signal handler for the SIGINT signal.
/// @return success/failure indication
///
/// @global sint
/// @global sterm
bool
install_signal_handlers(void)
{
  struct sigaction sa;
  sigset_t ss;
  int reti;

  log(LL_INFO, false, "main", "installing signal handlers");

  // Reset the signal indicator.
  sint  = false;
  sterm = false;

  // Create and apply a set of blocked signals. We exclude the two recognised
  // signals from this set.
  (void)sigfillset(&ss);
  (void)sigdelset(&ss, SIGINT);
  (void)sigdelset(&ss, SIGTERM);
  (void)sigprocmask(SIG_SETMASK, &ss, NULL);

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

  return true;
}

/// Block all signals. This is expected when using the pselect(2) call, that
/// enables a set of signals during the waiting period. No other system calls
/// or execution 
void
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

/// Create a signal mask that enables the SIGINT and SIGTERM signals.
void
create_signal_mask(sigset_t* mask)
{
  (void)sigfillset(mask);
  (void)sigdelset(mask, SIGSTOP);
  (void)sigdelset(mask, SIGKILL);
  (void)sigdelset(mask, SIGINT);
  (void)sigdelset(mask, SIGTERM);
}
