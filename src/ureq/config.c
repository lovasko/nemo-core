// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#include "common/log.h"
#include "common/parse.h"
#include "common/payload.h"
#include "ureq/funcs.h"
#include "ureq/types.h"
#include "ureq/version.h"


// Default values for optional arguments.
#define DEF_TARGET_COUNT   64         ///< Maximum of 64 targets.
#define DEF_COUNT          5          ///< Number of published datagrams.
#define DEF_INTERVAL       1000000000 ///< One second pause between payloads.
#define DEF_FINAL_WAIT     2000000000 ///< Two second wait time for responses.
#define DEF_UPDATE         60000000000 ///< One minute period of name resolution update.
#define DEF_TIME_TO_LIVE   32         ///< IP Time-To-Live value.
#define DEF_EXIT_ON_ERROR  false      ///< Process exit on publishing error.
#define DEF_LOG_LEVEL      LL_WARN    ///< Log errors and warnings by default.
#define DEF_LOG_COLOR      true       ///< Colors in the notification output.
#define DEF_MONOLOGUE      false      ///< Responses are recorded by default.
#define DEF_UDP_PORT       23000      ///< UDP port number to use
#define DEF_RECEIVE_BUFFER 2000000    ///< Socket receive buffer memory size.
#define DEF_SEND_BUFFER    2000000    ///< Socket send buffer memory size.
#define DEF_SILENT         false      ///< Do not suppress reporting.
#define DEF_BINARY         false      ///< CSV reporting by default.
#define DEF_GROUP          false      ///< Do not group requests.
#define DEF_KEY            0          ///< Issue promiscuous requests.
#define DEF_LENGTH         NEMO_PAYLOAD_SIZE

/// Print the usage information to the standard output stream.
static void
print_usage(void)
{
  (void)printf(
    "About:\n"
    "  Unicast network requester.\n"
    "  Program version: %d.%d.%d\n"
    "  Payload version: %d\n\n"

    "Usage:\n"
    "  ureq [OPTIONS] target [target]...\n\n"

    "Arguments:\n"
    "  target  IPv4/IPv6 address or hostname\n\n"

    "Options:\n"
    "  -4      Use only the IPv4 protocol.\n"
    "  -6      Use only the IPv6 protocol.\n"
    "  -c CNT  Number of requests to issue. (def=%d)\n"
    "  -e      Stop the process on first network error.\n"
    "  -g      Group requests at the start of each round.\n"
    "  -h      Print this help message.\n"
    "  -i DUR  Interval duration between published datagram rounds. (def=1s)\n"
    "  -j CNT  Upper limit on network target count. (def=%d)\n"
    "  -k KEY  Key for the current run. (def=%d)\n"
    "  -l LEN  Extended length of the payload. (def=%d)\n"
    "  -m      Do not react to responses (monologue mode).\n"
    "  -n      Turn off colors in logging messages.\n"
    "  -r RBS  Receive memory buffer size.\n"
    "  -s SBS  Send memory buffer size.\n"
    "  -p NUM  UDP port to use for all endpoints. (def=%d)\n"
    "  -t TTL  Set the Time-To-Live for all published datagrams. (def=%d)\n"
    "  -u DUR  Duration of the name resolution update period.\n"
    "  -v      Increase the verbosity of the logging output.\n"
    "  -w DUR  Wait time for responses after last request. (def=2s)\n",
    NEMO_REQ_VERSION_MAJOR,
    NEMO_REQ_VERSION_MINOR,
    NEMO_REQ_VERSION_PATCH,
    NEMO_PAYLOAD_VERSION,
    DEF_TARGET_COUNT,
    DEF_COUNT,
    DEF_KEY,
    DEF_LENGTH,
    DEF_UDP_PORT,
    DEF_TIME_TO_LIVE);
}

/// Select IPv4 protocol only.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input (unused)
static bool
option_4(struct config* cf, const char* in)
{
  (void)in;
  cf->cf_ipv4 = true;

  return true;
}

/// Select IPv6 protocol only.
/// @return success/failure indication
///
/// @param[out] cf  configuration
/// @param[in]  in argument input (unused)
static bool
option_6(struct config* cf, const char* in)
{
  (void)in;
  cf->cf_ipv6 = true;

  return true;
}

/// Attach a plugin from a shared object library. This function does not load
/// the plugins directly, it merely copies the arguments holding the paths to
/// the shared objects.
/// @return success/failure indication
///
/// @param[out] cf  configuration
/// @param[in]  in argument input
static bool
option_a(struct config* cf, const char* in)
{
  static uint64_t i = 0;

  if (i >= PLUG_MAX) {
    log(LL_WARN, false, "too many plugins, only %d allowed", PLUG_MAX);
    return false;
  }

  cf->cf_pi[i] = in;
  i++;

  return true;
}

/// Change the reporting to binary mode.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input (unused)
static bool
option_b(struct config* cf, const char* in)
{
  (void)in;
  cf->cf_bin = true;

  return true;
}

/// Set the number of emitted requests.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input
static bool
option_c(struct config* cf, const char* in)
{
  return parse_uint64(&cf->cf_cnt, in, 0, UINT64_MAX);
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

/// Group issued requests each round.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input (unused)
static bool
option_g(struct config* cf, const char* in)
{
  (void)in;
  cf->cf_grp = true;

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

/// Interval between rounds of issued requests.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input
static bool
option_i(struct config* cf, const char* in)
{
  return parse_scalar(&cf->cf_int, in, "ns", parse_time_unit);
}

static bool
option_j(struct config* cf, const char* in)
{
  // The maximal number of targets is such that an array of `struct target`
  // would be allocatable in memory. This would suggest the solution of
  // choosing SIZE_MAX divided by the size of the struct, but we need to
  // protect against the case where `uint64_t` is a smaller type than `size_t`.
  return parse_uint64(&cf->cf_ntg, in, 1,
    ((SIZE_MAX < UINT64_MAX ? SIZE_MAX : UINT64_MAX)
      / sizeof(struct target)) - 1);
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
  uint64_t len;
  bool retb;

  // Try to parse the length in bytes.
  retb = parse_scalar(&len, in, "b", parse_memory_unit);
  if (retb == false) {
    return false;
  }

  // Ensure that the value falls within the accepted interval.
  if (len < NEMO_PAYLOAD_SIZE || len > 65536) {
    log(LL_WARN, false, "length must be between %d and %d", NEMO_PAYLOAD_SIZE, 64436);
    return false;
  }

  cf->cf_len = len;
  return true;
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
/// @param[out] cf  configuration 
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
  return parse_scalar(&cf->cf_rbuf, in, "b", parse_memory_unit);
}

/// Set the socket send buffer size.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input
static bool
option_s(struct config* cf, const char* in)
{
  return parse_scalar(&cf->cf_sbuf, in, "b", parse_memory_unit);
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

/// Set the update period for name resolution.
/// @return success/failure indication
///
/// @param[out] cf cofiguration
/// @param[in]  in argument input
static bool
option_u(struct config* cf, const char* in)
{
  return parse_scalar(&cf->cf_rld, in, "ns", parse_time_unit);
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

/// Duration to wait for responses after the last request.
/// @return success/failure indication
///
/// @param[out] cf configuration
/// @param[in]  in argument input
static bool
option_w(struct config* cf, const char* in)
{
  return parse_scalar(&cf->cf_wait, in, "ns", parse_time_unit);
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
    cf->cf_pi[i] = NULL;
  }

  cf->cf_ntg  = DEF_TARGET_COUNT;
  cf->cf_cnt  = DEF_COUNT;
  cf->cf_int  = DEF_INTERVAL;
  cf->cf_wait = DEF_FINAL_WAIT;
  cf->cf_rbuf = DEF_RECEIVE_BUFFER;
  cf->cf_sbuf = DEF_SEND_BUFFER;
  cf->cf_err  = DEF_EXIT_ON_ERROR;
  cf->cf_port = DEF_UDP_PORT;
  cf->cf_ttl  = DEF_TIME_TO_LIVE;
  cf->cf_rld  = DEF_UPDATE;
  cf->cf_mono = DEF_MONOLOGUE;
  cf->cf_sil  = DEF_SILENT;
  cf->cf_bin  = DEF_BINARY;
  cf->cf_grp  = DEF_GROUP;
  cf->cf_key  = DEF_KEY;
  cf->cf_llvl = (log_lvl = DEF_LOG_LEVEL);
  cf->cf_lcol = (log_col = DEF_LOG_COLOR);
  cf->cf_ipv4 = false;
  cf->cf_ipv6 = false;

  return true;
}

/// Sanitize the IPv4/IPv6 options.
/// @return success/failure indication
///
/// @param[out] cf configuration
static bool
organize_protocols(struct config* cf)
{
  // Check whether two exclusive modes were selected.
  if (cf->cf_ipv4 == true && cf->cf_ipv6 == true) {
    log(LL_WARN, false, "options -4 and -6 are mutually exclusive");
    return false;
  }

  // If no restrictions on the IP version were set, enable both versions.
  if (cf->cf_ipv4 == false && cf->cf_ipv6 == false) {
    cf->cf_ipv4 = true;
    cf->cf_ipv6 = true;
  }

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
  struct option opts[22] = {
    { '4',  false, option_4 },
    { '6',  false, option_6 },
    { 'a',  true , option_a },
    { 'b',  false, option_b },
    { 'c',  true,  option_c },
    { 'e',  false, option_e },
    { 'g',  false, option_g },
    { 'h',  false, option_h },
    { 'i',  true , option_i },
    { 'j',  true , option_j },
    { 'k',  true , option_k },
    { 'l',  true , option_l },
    { 'm',  false, option_m },
    { 'n',  false, option_n },
    { 'p',  true , option_p },
    { 'q',  false, option_q },
    { 'r',  true , option_r },
    { 's',  true , option_s },
    { 't',  true , option_t },
    { 'u',  true,  option_u },
    { 'v',  false, option_v },
    { 'w',  true,  option_w }
  };

  log(LL_INFO, false, "parsing command-line options");

  (void)memset(optdsl, '\0', sizeof(optdsl));
  generate_getopt_string(optdsl, opts, 22);

  // Set optional arguments to sensible defaults.
  set_defaults(cf);

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
      log(LL_WARN, false, "unknown option %c", optopt);

      return false;
    }

    // Find the relevant option.
    for (i = 0; i < 22; i++) {
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
  if (optind == argc) {
    log(LL_WARN, false, "at least one target expected");
    return false;
  }

  // Verify that the number of arguments is below the limit.
  if ((uint64_t)(argc - optind) > cf->cf_ntg) {
    log(LL_WARN, false, "too many arguments, maximum is %d", cf->cf_ntg);
    return false;
  }

  // All remaining positional arguments are targets.
  cf->cf_tg = calloc((size_t)cf->cf_ntg, sizeof(char*));
  if (cf->cf_tg == NULL) {
    log(LL_WARN, true, "unable to allocate memory for targets");
    return false;
  }

  for (opt = optind; opt < argc; opt++) {
    cf->cf_tg[opt - optind] = argv[opt];
  }

  retb = organize_protocols(cf);
  if (retb == false) {
    return false;
  }

  // Assign the logging settings.
  log_lvl = cf->cf_llvl;
  log_col = cf->cf_lcol;

  return true;
}

/// Use the logging framework to output the selected option values used
/// throughout the run of the program. Moreover, it lists the implied
/// configuration details, such as the local UDP port numbers.
///
/// @param[in] cf configuration
void
log_config(const struct config* cf)
{
  log(LL_DEBUG, false, "responder UDP port: %" PRIu64, cf->cf_port);
  log(LL_DEBUG, false, "unique key: %" PRIu64, cf->cf_key);
  log(LL_DEBUG, false, "Time-To-Live: %" PRIu64, cf->cf_ttl);
  log(LL_DEBUG, false, "receive buffer size: %" PRIu64 " bytes", cf->cf_rbuf);
  log(LL_DEBUG, false, "send buffer size: %" PRIu64 " bytes", cf->cf_sbuf);
  log(LL_DEBUG, false, "monologue mode: %s", cf->cf_mono == true ? "yes" : "no");
  log(LL_DEBUG, false, "binary report: %s", cf->cf_bin == true ? "yes" : "no");
}
