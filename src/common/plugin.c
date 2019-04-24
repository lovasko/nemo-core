// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>

#include "common/log.h"
#include "common/payload.h"
#include "common/plugin.h"
#include "ures/funcs.h"
#include "ures/types.h"


/// Count the number of selected plugins.
/// @return number of plugins
///
/// @param[in] so shared object file paths
static uint64_t
count_plugins(const char* so[])
{
  uint64_t i;

  for (i = 0; i < PLUG_MAX; i++) {
    if (so[i] == NULL) {
      break;
    }
  }

  return i;
}

/// Report an error related to the dynamic library subsystem.
///
/// @param[in] name object name
static void
report_error(const char* name)
{
  char err[128];

  (void)memset(err, '\0', sizeof(err));
  (void)strncpy(err, "unable to open %s:", 64);
  (void)strncat(err, dlerror(), 63);
  log(LL_WARN, false, err, name);
}

/// Dynamically load the shared objects and extract all necessary symbols that
/// define the plugin from it.
/// @return success/failure indication
///
/// @param[out] pi  plugins
/// @param[out] npi number of plugins
/// @param[in]  so  shared object file paths
bool
load_plugins(struct plugin* pi, uint64_t* npi, const char* so[])
{
  uint64_t i;

  *npi = count_plugins(so);

  for (i = 0; i < *npi; i++) {
    // Open the shared object library.
    pi[i].pi_hndl = dlopen(so[i], RTLD_NOW);
    if (pi[i].pi_hndl == NULL) {
      report_error(so[i]);
      return false;
    }

    // Load the plugin name.
    pi[i].pi_name = dlsym(pi[i].pi_hndl, "nemo_name");
    if (pi[i].pi_name == NULL) {
      report_error("nemo_name");
      return false;
    }

    // Load the initialisation procedure.
    pi[i].pi_init = dlsym(pi[i].pi_hndl, "nemo_init");
    if (pi[i].pi_init == NULL) {
      report_error("nemo_init");
      return false;
    }

    // Load the main procedure to execute upon the response event.
    pi[i].pi_evnt = dlsym(pi[i].pi_hndl, "nemo_evnt");
    if (pi[i].pi_evnt == NULL) {
      report_error("nemo_evnt");
      return false;
    }

    // Load the clean-up procedure.
    pi[i].pi_free = dlsym(pi[i].pi_hndl, "nemo_free");
    if (pi[i].pi_free == NULL) {
      report_error("nemo_free");
      return false;
    }
  }

  return true;
}

/// Continuously read payloads from the pipe, blocking when no data is
/// available.
///
/// @param[in] pi plugin
static void
read_loop(const struct plugin* pi)
{
  struct payload pl;
  ssize_t retss;

  while (true) {
    // Read a payload from the pipe.
    retss = read(pi->pi_pipe[0], &pl, sizeof(pl));
    
    // Check for errors.
    if (retss == -1) {
      log(LL_WARN, true, "unable to read payload from a pipe");
      return;
    }

    // Check whether a full payload was dequeued.
    if (retss != (ssize_t)sizeof(pl)) {
      log(LL_WARN, false, "unable to read full payload from a pipe");
      return;
    }

    // Execute the plugin event action based on the payload content.
    pi->pi_evnt(pl.pl_reqk, pl.pl_reqk, pl.pl_reqk, pl.pl_reqk);
  }
}

/// Close the appropriate end of the pipe depending on which forked process
/// executes this function.
/// @return success/failure indication
///
/// @param[in] pin plugin
static bool
close_pipe_end(const struct plugin* pin)
{
  int reti;

  // If we are within the child plugin process, close the writing end of the
  // pipe, as the plugins do not communicate back to the main responder
  // process. This also decrements the reference count on the writing end back
  // to pre-fork one, meaning that the pipe will be disposed of once the main
  // process closes the end.
  if (pin->pi_pid == 0) {
    reti = close(pin->pi_pipe[1]);
    if (reti == -1) {
      log(LL_WARN, true, "unable to close the writing end of the pipe");
      return false;
    }
  } else {
    // If we are within the parent responder process, close the reading end of
    // the pipe, as the parent process does not expect communication from the
    // plugins.
    reti = close(pin->pi_pipe[0]);
    if (reti == -1) {
      log(LL_WARN, true, "unable to close the reading end of the pipe");
      return false;
    }
  }

  return true;
}

/// Start all plugins.
/// @return success/failure indication
///
/// @param[out] pi  array of plugins
/// @param[in]  npi number of plugins
bool
start_plugins(struct plugin* pi, const uint64_t npi)
{
  uint64_t i;
  int reti;
  bool retb;
  int fl;

  for (i = 0; i < npi; i++) {
    // Create a pipe through which the processes will communicate.
    reti = pipe(pi[i].pi_pipe);
    if (reti == -1) {
      log(LL_WARN, true, "unable to create a pipe");
      return false;
    }

    // Obtain the file status flags of the writing end of the pipe.
    fl = fcntl(pi[i].pi_pipe[0], F_GETFL);
    if (fl == -1) {
      log(LL_WARN, true, "unable to obtain file status flags for pipe");
      return false;
    }

    // Set the writing end of the pipe to be non-blocking, so that a slow
    // plugin does not block the main program (and other plugins).
    fl |= O_NONBLOCK;
    reti = fcntl(pi[i].pi_pipe[0], F_SETFL, fl);
    if (reti == -1) {
      log(LL_WARN, true, "unable to set the pipe to be non-blocking");
      return false;
    }

    // Create a new process.
    pi[i].pi_pid = fork();

    // Check for error first.
    if (pi[i].pi_pid == -1) {
      log(LL_WARN, true, "unable to start a plugin process");
      return false;
    }

    // Close the appropriate end of the pipe in each process.
    retb = close_pipe_end(&pi[i]);
    if (retb == false) {
      return false;
    }

    // In case this code is executed in the child process start the main event
    // loop.
    if (pi[i].pi_pid == 0) {
      retb = pi[i].pi_init();
      if (retb == false) {
        log(LL_WARN, false, "unable to initialise plugin %s", pi[i].pi_name);
        return false;
      }

      // Start the main process loop that awaits incoming payloads.
      read_loop(&pi[i]);

      // Perform the final clean-up and exit the process.
      pi[i].pi_free();
      exit(EXIT_SUCCESS);
    }
  }

  return true;
}

/// Perform the event action. This is done by sending a payload via a pipe
/// to the plugin process. The plugin process has a continuous read loop and
/// executes the appropriate plugin action.
///
/// @param[in] pi  array of plugins
/// @param[in] npi number of plugins
/// @param[in] pl  payload
void
notify_plugins(const struct plugin* pi,
               const uint64_t npi,
               const struct payload* pl)
{
  uint64_t i;
  ssize_t retss;

  for (i = 0; i < npi; i++) {
    retss = write(pi[i].pi_pipe[0], pl, sizeof(*pl));

    // Check for error.
    if (retss == -1) {
      log(LL_WARN, true, "unable to send payload to plugin %s", pi[i].pi_name);
    }

    // Check whether all expected data was written to the pipe.
    if (retss != (ssize_t)sizeof(*pl)) {
      log(LL_WARN, false, "unable to send full payload to plugin %s", pi[i].pi_name);
    }
  }
}

/// Log debugging information about an exit of a child process (plugin).
///
/// @param[in] ws wait status
static void
log_exit_details(const int ws)
{
  // Check for orderly exit.
  if (WIFEXITED(ws)) {
    log(LL_DEBUG, false, "plugin exited with code: %d", WEXITSTATUS(ws));
    return;
  }

  // Check whether it was killed by a signal.
  if (WIFSIGNALED(ws)) {
    log(LL_DEBUG, false, "plugin killed by signal: %s", strsignal(WTERMSIG(ws)));
    return;
  }
}

/// Terminate all plugins. This is done by closing the appropriate pipe,
/// causing the read loop to terminate. This in turn triggers the clean-up
/// procedure defined for the plugin. The main process waits for the process
/// to finish.
///
/// @param[in] pi  array of plugins
/// @param[in] npi number of plugins
void
terminate_plugins(const struct plugin* pi, const uint64_t npi)
{
  uint64_t i;
  int reti;
  int ws;

  // The loop does not terminate when a particular clean-up routine fails,
  // as the process is already about to terminate. This way all plugins get
  // a chance to perform an orderly clean-up.
  for (i = 0; i < npi; i++) {
    reti = close(pi[i].pi_pipe[1]);
    if (reti == -1) {
      log(LL_WARN, true, "unable to close a pipe");
    }
  }

  // Once the pipe was closed, the main loop of the plugin process should come
  // to end. The final wait on the child process will ensure that the resource
  // usage data gets included in the final report for the main process.
  for (i = 0; i < npi; i++) {
    reti = waitpid(pi[i].pi_pid, &ws, 0);
    if (reti == -1) {
      log(LL_WARN, true, "unable to wait for plugin %s", pi[i].pi_name);
      continue;
    }

   log_exit_details(ws);
  }
}
