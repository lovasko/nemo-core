// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "common/log.h"
#include "common/parse.h"
#include "ures/funcs.h"
#include "ures/types.h"
#include "ures/version.h"


// Default values.
#define DEF_RECEIVE_BUFFER_SIZE 2000000
#define DEF_SEND_BUFFER_SIZE    2000000
#define DEF_EXIT_ON_ERROR       false
#define DEF_UDP_PORT            23000
#define DEF_LOG_LEVEL           LL_WARN
#define DEF_LOG_COLOR           true
#define DEF_TIME_TO_LIVE        64
#define DEF_MONOLOGUE           false
#define DEF_SILENT              false
#define DEF_KEY                 0
#define DEF_TIMEOUT             0
#define DEF_LENGTH              0
#define DEF_PROTO_VERSION_4     true

/// Print the usage information to the standard output stream.
static void
print_usage(void)
{
  (void)printf(
    "About:\n"
    "  Unicast network responder.\n"
    "  Program version: %d.%d.%d\n"
    "  Payload version: %d\n\n"

    "Usage:\n"
    "  ures [OPTIONS]\n\n"

    "Options:\n"
    "  -6      Use the IPv6 protocol.\n"
    "  -a OBJ  Attach a plugin from a shared object file.\n"
    "  -d DUR  Time-out for lack of incoming requests.\n"
    "  -e      Stop the process on first transmission error.\n"
    "  -h      Print this help message.\n"
    "  -k KEY  Unique key for identification of payloads.\n"
    "  -l LEN  Overall accepted payload length.\n"
    "  -m      Disable responding (monologue mode).\n"
    "  -n      Turn off coloring in the logging output.\n"
    "  -p NUM  UDP port to use for all endpoints. (def=%d)\n"
    "  -q      Suppress reporting to standard output.\n"
    "  -r RBS  Socket receive memory buffer size. (def=2m)\n"
    "  -s SBS  Socket send memory buffer size. (def=2m)\n"
    "  -t TTL  Outgoing IP Time-To-Live value. (def=%d)\n"
    "  -v      Increase the verbosity of the logging output.\n",
    NEMO_RES_VERSION_MAJOR,
    NEMO_RES_VERSION_MINOR,
    NEMO_RES_VERSION_PATCH,
    NEMO_PAYLOAD_VERSION,
    DEF_UDP_PORT,
    DEF_TIME_TO_LIVE);
}

/// Select IPv6 protocol only.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input (unused)
static bool
option_6(struct config* cf, const char* in)
{
  (void)in;
  cf->cf_ipv4 = false;

  return true;
}

/// Attach a plugin from a shared object library. This function does not load
/// the plugins directly, it merely copies the arguments holding the paths to
/// the shared objects.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input
static bool
option_a(struct config* cf, const char* in)
{
  static uint64_t i = 0;

  if (i >= PLUG_MAX) {
    log(LL_WARN, false, "too many plugins, only %d allowed", PLUG_MAX);
    return false;
  }

  cf->cf_plgs[i] = in;
  i++;

  return true;
}

/// Time-out of inactivity (no requests received).
/// @return success/failure indication
///
/// @param[in] cf configuration
/// @param[in] in argument input
static bool
option_d(struct config* cf, const char* in)
{
  return parse_scalar(&cf->cf_ito, in, "ns", 1, UINT64_MAX, parse_time_unit);
}

/// Terminate the process on first network-related error.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input (unused)
static bool
option_e(struct config* cf, const char* in)
{
  (void)in;
  cf->cf_err = true;

  return true;
}

/// Print the usage help message and exit the process.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input (unused)
static bool
option_h(struct config* cf, const char* in)
{
  (void)cf;
  (void)in;

  print_usage();
  exit(EXIT_FAILURE);

  return true;
}

/// Set a unique key to identify the flow.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input
static bool
option_k(struct config* cf, const char* in)
{
  return parse_uint64(&cf->cf_key, in, 1, UINT64_MAX);
}

/// Overall length of the payload (includes mandatory and optional parts).
/// @return success/failure indication
///
/// @param[in] cf configuration
/// @param[in] in argument input
static bool
option_l(struct config* cf, const char* in)
{
  return parse_scalar(&cf->cf_len, in, "b", NEMO_PAYLOAD_SIZE, 64436, parse_memory_unit);
}

/// Enable monologue mode where no responses are being issued.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input (unused)
static bool
option_m(struct config* cf, const char* in)
{
  (void)in;
  cf->cf_mono = true;

  return true;
}

/// Turn off coloring and highlights in the logging output.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input (unused)
static bool
option_n(struct config* cf, const char* in)
{
  (void)in;
  cf->cf_lcol = false;

  return true;
}

/// Set the UDP port number used for all communication.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input
static bool
option_p(struct config* cf, const char* in)
{
  return parse_uint64(&cf->cf_port, in, 1, 65535);
}

/// Suppress reporting to the standard output stream.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input (unused)
static bool
option_q(struct config* cf, const char* in)
{
  (void)in;
  cf->cf_sil = true;

  return true;
}

/// Set the socket receive buffer size.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input
static bool
option_r(struct config* cf, const char* in)
{
  return parse_scalar(&cf->cf_rbuf, in, "b", NEMO_PAYLOAD_SIZE, SIZE_MAX, parse_memory_unit);
}

/// Set the socket send buffer size.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input
static bool
option_s(struct config* cf, const char* in)
{
  return parse_scalar(&cf->cf_sbuf, in, "b", NEMO_PAYLOAD_SIZE, SIZE_MAX, parse_memory_unit);
}

/// Set the IP Time-To-Live value.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input
static bool
option_t(struct config* cf, const char* in)
{
  return parse_uint64(&cf->cf_ttl, in, 1, 255);
}

/// Increase the logging verbosity.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input (unused)
static bool
option_v(struct config* cf, const char* in)
{
  (void)in;

  if (cf->cf_llvl == LL_DEBUG) {
    cf->cf_llvl = LL_TRACE;
  }

  if (cf->cf_llvl == LL_INFO) {
    cf->cf_llvl = LL_DEBUG;
  }

  if (cf->cf_llvl == LL_WARN) {
    cf->cf_llvl = LL_INFO;
  }

  if (cf->cf_llvl == LL_ERROR) {
    cf->cf_llvl = LL_WARN;
  }

  return true;
}

/// Assign default values to all options.
/// @return success/failure indication
///
/// @param[out] cf configuration
static bool
set_defaults(struct config* cf)
{
  intmax_t i;

  for (i = 0; i < PLUG_MAX; i++) {
    cf->cf_plgs[i] = NULL;
  }

  cf->cf_rbuf = DEF_RECEIVE_BUFFER_SIZE;
  cf->cf_sbuf = DEF_SEND_BUFFER_SIZE;
  cf->cf_err  = DEF_EXIT_ON_ERROR;
  cf->cf_port = DEF_UDP_PORT;
  cf->cf_ttl  = DEF_TIME_TO_LIVE;
  cf->cf_mono = DEF_MONOLOGUE;
  cf->cf_sil  = DEF_SILENT;
  cf->cf_llvl = (log_lvl = DEF_LOG_LEVEL);
  cf->cf_lcol = (log_col = DEF_LOG_COLOR);
  cf->cf_ipv4 = DEF_PROTO_VERSION_4;
  cf->cf_key  = DEF_KEY;
  cf->cf_ito  = DEF_TIMEOUT;
  cf->cf_len  = DEF_LENGTH;

  return true;
}

/// Generate the getopt(3) DSL based on a set of option definitions.
///
/// @param[out] str   DSL string
/// @param[in]  opts  array of option definitions
/// @param[in]  nopts number of option definitions
static void
generate_getopt_string(char* str,
                       const struct option* opts,
                       const uint64_t nopts)
{
  uint64_t sidx; // Index in the generated string.
  uint64_t oidx; // Index in the options array.

  sidx = 0;
  for (oidx = 0; oidx < nopts; oidx++) {
    // Append the option name.
    str[sidx] = opts[oidx].op_name;
    sidx++;

    // Append a colon if the option expects an argument.
    if (opts[oidx].op_arg == true) {
      str[sidx] = ':';
      sidx++;
    }
  }
}

/// Parse configuration defined as command-line options.
/// @return success/failure indication
///
/// @param[out] cf   configuration
/// @param[in]  argc argument count
/// @param[in]  argv argument vector
bool
parse_config(struct config* cf, int argc, char* argv[])
{
  int opt;
  bool retb;
  uint64_t i;
  char optdsl[128];
  struct option opts[15] = {
    { '6',  false, option_6 },
    { 'a',  true , option_a },
    { 'd',  true,  option_d },
    { 'e',  false, option_e },
    { 'h',  false, option_h },
    { 'k',  true , option_k },
    { 'l',  true , option_l },
    { 'm',  false, option_m },
    { 'n',  false, option_n },
    { 'p',  true , option_p },
    { 'q',  false, option_q },
    { 'r',  true , option_r },
    { 's',  true , option_s },
    { 't',  true , option_t },
    { 'v',  false, option_v }
  };

  log(LL_INFO, false, "parsing command-line options");

  (void)memset(optdsl, '\0', sizeof(optdsl));
  generate_getopt_string(optdsl, opts, 15);

  // Set optional arguments to sensible defaults.
  retb = set_defaults(cf);
  if (retb == false) {
    log(LL_WARN, false, "unable to set option defaults");
    return false;
  }

  // Loop through available options.
  while (true) {
    // Parse the next option.
    opt = getopt(argc, argv, optdsl);
    if (opt == -1) {
      break;
    }

    // Unknown option.
    if (opt == '?') {
      print_usage();
      log(LL_WARN, false, "unknown option '%c'", optopt);

      return false;
    }

    // Find the relevant option.
    for (i = 0; i < 15; i++) {
      if (opts[i].op_name == (char)opt) {
        retb = opts[i].op_act(cf, optarg);
        if (retb == false) {
          log(LL_WARN, false, "action for option '%c' failed", opt);
          return false;
        }

        break;
      }
    }
  }

  // Verify that there are no positional arguments.
  if (optind != argc) {
    log(LL_WARN, false, "no arguments are expected");
    return false;
  }

  // Assign the logging settings.
  log_lvl = cf->cf_llvl;
  log_col = cf->cf_lcol;

  return true;
}

/// Use the logging framework to output the selected option values used
/// throughout the run of the program.
///
/// @param[in] cf configuration
void
log_config(const struct config* cf)
{
  const char* mono;
  const char* ipv;
  const char* err;
  char key[32];
  char len[32];
  char ito[32];

  // Monologue mode.
  if (cf->cf_mono == true) {
    mono = "yes";
  } else {
    mono = "no";
  }

  // Internet protocol version.
  if (cf->cf_ipv4 == true) {
    ipv = "IPv4";
  } else {
    ipv = "IPv6";
  }

  // Exit on error.
  if (cf->cf_err == true) {
    err = "yes";
  } else {
    err = "no";
  }

  // Key.
  if (cf->cf_key == 0) {
    (void)strncpy(key, "any", sizeof(key));
  } else {
    (void)snprintf(key, sizeof(key), "%" PRIu64, cf->cf_key);
  }

  // Payload length.
  if (cf->cf_len == 0) {
    (void)strncpy(len, "any", sizeof(len));
  } else {
    (void)snprintf(len, sizeof(len), "%" PRIu64 "B", cf->cf_len);
  }

  // Inactivity timeout.
  if (cf->cf_ito == 0) {
    (void)strncpy(ito, "infinity", sizeof(len));
  } else {
    (void)snprintf(ito, sizeof(ito), "%" PRIu64, cf->cf_ito);
  }

  log(LL_DEBUG, false, "UDP port: %" PRIu64, cf->cf_port);
  log(LL_DEBUG, false, "unique key: %s", key);
  log(LL_DEBUG, false, "time-to-live: %" PRIu64, cf->cf_ttl);
  log(LL_DEBUG, false, "inactivity timeout: %s", ito);
  log(LL_DEBUG, false, "payload length: %s", len);
  log(LL_DEBUG, false, "send buffer size: %" PRIu64 "%c", cf->cf_sbuf, 'B');
  log(LL_DEBUG, false, "receive buffer size: %" PRIu64 "%c", cf->cf_sbuf, 'B');
  log(LL_DEBUG, false, "internet protocol version: %s", ipv);
  log(LL_DEBUG, false, "exit on error: %s", err);
  log(LL_DEBUG, false, "monologue mode: %s", mono);
}
