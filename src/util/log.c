// Copyright (c) 2018 Daniel Lovasko.
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

#include "log.h"


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
      delim = strpbrk(pcent, "cdfghopsu");
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
log_(const uint8_t lvl, const bool perr, const char* fmt, ...)
{
  char tstr[32];
  char hfmt[NEMO_LOG_MAX_LENGTH];
  char msg[NEMO_LOG_MAX_LENGTH];
  char errmsg[NEMO_LOG_MAX_LENGTH];
  struct tm* tfmt;
  struct timespec tspec;
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
  clock_gettime(CLOCK_REALTIME, &tspec);
  tfmt = gmtime(&tspec.tv_sec);
  strftime(tstr, sizeof(tstr), "%T", tfmt);

  // Prepare highlights for the message variables.
  memset(hfmt, '\0', sizeof(hfmt));
  if (log_col)
    highlight(hfmt, fmt);
  else
    memcpy(hfmt, fmt, strlen(fmt));

  // Fill in the passed message.
  va_start(args, fmt);
  vsprintf(msg, hfmt, args);
  va_end(args);

  // Obtain the errno message.
  memset(errmsg, '\0', sizeof(errmsg));
  if (perr)
    sprintf(errmsg, ": %s", strerror(save));

  // Format the level name.
  memset(lstr, '\0', sizeof(lstr));
  if (log_col)
    sprintf(lstr, "\x1b[%dm%s\x1b[0m", lcol[lvl], lname[lvl]);
  else
    memcpy(lstr, lname[lvl], strlen(lname[lvl]));

  // Print the final log line.
  (void)fprintf(stderr, "[%s.%03" PRIu32 "] %s - %s%s\n",
                tstr, (uint32_t)tspec.tv_nsec / 1000000, lstr, msg, errmsg);
}
