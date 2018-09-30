// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <dlfcn.h>
#include <string.h>

#include "common/payload.h"
#include "res/types.h"
#include "res/proto.h"
#include "util/log.h"


/// Count the number of selected plugins.
/// @return number of plugins
///
/// @param[in] opts command-line options
static uint64_t
count_plugins(const struct options* opts)
{
  uint64_t res;

  for (res = 0; res < PLUG_MAX; res++)
    if (opts->op_plgs[res] == NULL)
      break;

  return res;
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
  log(LL_WARN, false, "main", err, name);
}

/// Dynamically load the shared objects and extract all necessary symbols that
/// define the plugin from it.
/// @return success/failure indication
///
/// @param[out] pins  array of plugins
/// @param[out] npins number of plugins
/// @param[in]  opts  command-line options
bool
load_plugins(struct plugin* pins,
             uint64_t* npins,
             const struct options* opts)
{
  uint64_t i;

  *npins = count_plugins(opts);

  for (i = 0; i < *npins; i++) {
    // Open the shared object library.
    pins[i].pi_hndl = dlopen(opts->op_plgs[i], RTLD_NOW);
    if (pins[i].pi_hndl == NULL) {
      report_error(opts->op_plgs[i]);
      return false;
    }

    // Load the plugin name.
    pins[i].pi_name = dlsym(pins[i].pi_hndl, "nemo_name");
    if (pins[i].pi_name == NULL) {
      report_error("nemo_name");
      return false;
    }

    // Load the initialisation procedure.
    pins[i].pi_init = dlsym(pins[i].pi_hndl, "nemo_init");
    if (pins[i].pi_init == NULL) {
      report_error("nemo_init");
      return false;
    }

    // Load the main procedure to execute upon the response event.
    pins[i].pi_evnt = dlsym(pins[i].pi_hndl, "nemo_evnt");
    if (pins[i].pi_evnt == NULL) {
      report_error("nemo_evnt");
      return false;
    }

    // Load the clean-up procedure.
    pins[i].pi_free = dlsym(pins[i].pi_hndl, "nemo_free");
    if (pins[i].pi_free == NULL) {
      report_error("nemo_free");
      return false;
    }
  }

  return true;
}

/// Start all plugins.
/// @return success/failure indication
///
/// @param[in] pins  array of plugins
/// @param[in] npins number of plugins
bool 
start_actions(const struct plugin* pins, const uint64_t npins)
{
  uint64_t i;
  bool retb;

  for (i = 0; i < npins; i++) {
    retb = pins[i].pi_init();
    if (retb == false) {
      log(LL_WARN, false, "main", "unable to initialize plugin %s",
        pins[i].pi_name);
      return false;
    }
  }

  return true;
}

/// Perform the event action.
///
/// @param[in] pins  array of plugins
/// @param[in] npins number of plugins
/// @param[in] pl    payload
void
exec_plugins(const struct plugin* pins,
             const uint64_t npins,
             const payload* pl)
{
  uint64_t i;
  bool retb;

  for (i = 0; i < npins; i++) {
    retb = pins[i].pi_evnt(pl->pl_resk, pl->pl_reqk, pl->pl_reqk, pl->pl_reqk);
    if (retb == false)
      log(LL_WARN, false, "main", "unable to execute plugin %s",
        pins[i].pi_name);
  }
}

/// Perform clean-up of all plugins.
///
/// @param[in] pins  array of plugins
/// @param[in] npins number of plugins
void
free_plugins(const struct plugin* pins, const uint64_t npins)
{
  uint64_t i;
  bool retb;

  // The loop does not terminate when a particular clean-up routine fails, as
  // the process is already about to terminate. This way all plugins get a
  // chance to perform an orderly clean-up.
  for (i = 0; i < npins; i++) {
    retb = pins[i].pi_free();
    if (retb == false)
      log(LL_WARN, false, "main", "clean-up of plugin %s failed",
        pins[i].pi_name);
  }
}
