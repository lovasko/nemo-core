// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>

#include "common/log.h"


// Logging settings.
uint8_t log_lvl; ///< Minimal logging level.
bool    log_col; ///< Coloring policy.

// Maximal length of a logging line.
#define NEMO_LOG_MAX_LENGTH 128

/// Add ASCII escape sequences to highlight every substitution in a
/// printf-formatted string.
///
/// @param[out] out highlighted string
/// @param[in]  inp input string
static void
highlight(char* out, const char* inp)
{
  char* pcent;
  char* delim;
  char* str;
  char cpy[NEMO_LOG_MAX_LENGTH];
  char hold;

  // As the input string is likely to be a compiler literal, and therefore
  // is stored in a read-only memory, we have to make a copy of it.
  memset(cpy, '\0', sizeof(cpy));
  strcpy(cpy, inp);

  hold = '\0';
  str = cpy;
  while (str != NULL) {
    // Find the next substitution.
    pcent = strchr(str, '%');
    if (pcent != NULL) {
      // Copy the text leading to the start of the substitution.
      *pcent = '\0';
      strcat(out, str);

      // Append the ASCII escape code to start the highlighting.
      strcat(out, "\x1b[1m");

      // Locate the substitution's end and copy the contents.
      *pcent = '%';
      delim = strpbrk(pcent, "cdfghopsux");
      if (delim != NULL) {
        delim++;
        hold = *delim;
        *delim = '\0';
      }
      strcat(out, pcent);

      // Insert the ASCII escape code to end the highlighting.
      strcat(out, "\x1b[0m");

      // Move back the space and continue.
      if (delim != NULL) {
        *delim = hold;
        str = delim;
      } else {
        str = NULL;
      }
    } else {
      // Since there are no more substitutions in the input string, copy the
      // rest of it and finish.
      strcat(out, str);
      break;
    }
  }
}

/// Issue a log line to the selected back-end service.
///
/// @param[in] lvl  logging level (one of LL_*)
/// @param[in] perr append the errno string to end of the log line
/// @param[in] fmt  message to log
/// @param[in] ...  arguments for the message
void
log(const uint8_t lvl,
    const bool perr,
    const char* fmt,
    ...)
{
  char tstr[32];
  char hfmt[NEMO_LOG_MAX_LENGTH];
  char msg[NEMO_LOG_MAX_LENGTH];
  char errmsg[NEMO_LOG_MAX_LENGTH];
  struct tm tfmt;
  time_t tspec;
  va_list args;
  int save;
  static const char* lname[] = {"ERROR", " WARN", " INFO", "DEBUG", "TRACE"};
  static const int lcol[]    = {31, 33, 32, 34, 35};
  char lstr[32];

  // Ignore messages that fall below the global threshold.
  if (lvl > log_lvl)
    return;

  // Save the errno with which the function was called.
  save = errno;

  // Obtain and format the current time in GMT.
  tspec = time(NULL);
  (void)gmtime_r(&tspec, &tfmt);
  (void)strftime(tstr, sizeof(tstr), "%F %T", &tfmt);

  // Prepare highlights for the message variables.
  (void)memset(hfmt, '\0', sizeof(hfmt));
  if (log_col == true)
    highlight(hfmt, fmt);
  else
    (void)memcpy(hfmt, fmt, strlen(fmt));

  // Fill in the passed message.
  va_start(args, fmt);
  (void)vsprintf(msg, hfmt, args);
  va_end(args);

  // Obtain the errno message.
  (void)memset(errmsg, '\0', sizeof(errmsg));
  if (perr == true)
    (void)sprintf(errmsg, ": %s", strerror(save));

  // Format the level name.
  (void)memset(lstr, '\0', sizeof(lstr));
  if (log_col == true)
    (void)sprintf(lstr, "\x1b[%dm%s\x1b[0m", lcol[lvl], lname[lvl]);
  else
    (void)memcpy(lstr, lname[lvl], strlen(lname[lvl]));

  // Print the final log line.
  (void)fprintf(stderr, "[%s] %s - %s%s\n", tstr, lstr, msg, errmsg);
}
