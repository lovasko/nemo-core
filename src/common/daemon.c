// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "common/daemon.h"
#include "common/log.h"


/// Redirect all standard streams to /dev/null.
/// @return success/failure indication
static bool
redirect_streams(void)
{
  int reti;

  // Close the standard streams.
  reti = close(STDIN_FILENO);
  if (reti == -1) {
    log(LL_WARN, true, "unable to close standard input stream");
    return false;
  }

  reti = close(STDOUT_FILENO);
  if (reti == -1) {
    log(LL_WARN, true, "unable to close standard output stream");
    return false;
  }

  reti = close(STDERR_FILENO);
  if (reti == -1) {
    log(LL_WARN, true, "unable to close standard error stream");
    return false;
  }

  // Re-open the standard streams as /dev/null files.
  reti = open("/dev/null", O_RDWR);
  if (reti < 0) {
    log(LL_WARN, true, "unable to redirect standard input");
    return false;
  }

  reti = open("/dev/null", O_RDWR);
  if (reti < 0) {
    log(LL_WARN, true, "unable to redirect standard output");
    return false;
  }

  reti = open("/dev/null", O_RDWR);
  if (reti < 0) {
    log(LL_WARN, true, "unable to redirect standard error");
    return false;
  }

  return true;
}

/// Create an copy of the process and terminate the older instance.
/// @return success/failure indication
static bool
fork_and_exit(void)
{
  pid_t pid;

  pid = fork();
  if (pid == -1) {
    log(LL_WARN, true, "unable to fork the process");
    return false;
  }

  // If the PID is non-zero, we have received the PID of our newly created
  // child process. Therefore, we terminate the process, as we only want to
  // keep the child process.
  if (pid != 0)
    exit(EXIT_SUCCESS);

  return true;
}

/// Turn the process into a daemon.
/// @return success/failure indication
bool
turn_into_daemon(void)
{
  int reti;
  bool retb;
  pid_t sid;

  // Kill the parent process, so that the terminal that started that process
  // receives a successful exit notification.
  retb = fork_and_exit();
  if (retb == false)
    return false;

  // Disassociate from the controlling tty, create a new session and become a
  // leader of it.
  sid = setsid();
  if (sid == -1) {
    log(LL_WARN, true, "unable to create a new session");
    return false;
  }

  // Fork again so that we can't have a tty attached to us later on.
  retb = fork_and_exit();
  if (retb == false)
    return false;

  // Change the working directory of the process to root. This is done so that
  // our process does not prevent unmounting of a particular file system.
  reti = chdir("/");
  if (reti == -1) {
    log(LL_WARN, true, "unable to change the working directory to root");
    return false;
  }

  // Empty the file creation mask.
  (void)umask(0);

  // Redirect standard streams.
  retb = redirect_streams();
  if (retb == false)
    return false;

  return true;
}
