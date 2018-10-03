// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "common/version.h"
#include "res/proto.h"
#include "res/types.h"
#include "util/log.h"
#include "util/parse.h"


// Default values.
#define DEF_RECEIVE_BUFFER_SIZE 2000000
#define DEF_SEND_BUFFER_SIZE    2000000
#define DEF_EXIT_ON_ERROR       false
#define DEF_UDP_PORT            23000
#define DEF_LOG_LEVEL           LL_WARN
#define DEF_LOG_COLOR           true
#define DEF_TIME_TO_LIVE        64
#define DEF_MONOLOGUE           false
#define DEF_DAEMON              false
#define DEF_SILENT              false
#define DEF_BINARY              false

/// Print the usage information to the standard output stream.
static void
print_usage(void)
{
  printf(
    "About:\n"
    "  Unicast network responder.\n"
    "  Program version: %d.%d.%d\n"
    "  Payload version: %d\n\n"

    "Usage:\n"
    "  nres [-46dehmnqv] [-a OBJ] [-k KEY] [-p NUM] [-r RBS] [-s SBS] "
    "[-t TTL]\n\n"

    "Options:\n"
    "  -4      Use only the IPv4 protocol.\n"
    "  -6      Use only the IPv6 protocol.\n"
    "  -a OBJ  Path to shared object file of an action set.\n"
    "  -b      Reporting data to be in binary format.\n"
    "  -d      Run the process as a daemon.\n"
    "  -e      Stop the process on first transmission error.\n"
    "  -h      Print this help message.\n"
    "  -k KEY  Key for the current run. (def=random)\n"
    "  -m      Disable responding (monologue mode).\n"
    "  -n      Turn off coloring in the logging output.\n"
    "  -p NUM  UDP port to use for all endpoints. (def=%d)\n"
    "  -q      Suppress reporting to standard output.\n"
    "  -r RBS  Socket receive memory buffer size. (def=2m)\n"
    "  -s SBS  Socket send memory buffer size. (def=2m)\n"
    "  -t TTL  Outgoing IP Time-To-Live value. (def=%d)\n"
    "  -v      Increase the verbosity of the logging output.\n",
    NEMO_VERSION_MAJOR,
    NEMO_VERSION_MINOR,
    NEMO_VERSION_PATCH,
    NEMO_PAYLOAD_VERSION,
    DEF_UDP_PORT,
    DEF_TIME_TO_LIVE);
}

/// Select IPv4 protocol only.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input (unused)
static bool 
option_4(struct options* opts, const char* inp)
{
  (void)inp;
  opts->op_ipv4 = true;

  return true;
}

/// Select IPv6 protocol only.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input (unused)
static bool
option_6(struct options* opts, const char* inp)
{
  (void)inp;
  opts->op_ipv6 = true;

  return true;
}

/// Attach a plugin from a shared object library. This function does not load
/// the plugins directly, it merely copies the arguments holding the paths to
/// the shared objects.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input
static bool
option_a(struct options* opts, const char* inp)
{
  static uint64_t i = 0;

  if (i >= PLUG_MAX) {
    log(LL_WARN, false, "main", "too many plugins, only %d allowed", PLUG_MAX);
    return false;
  }

  opts->op_plgs[i] = inp;
  i++;

  return true;
}

/// Change the reporting to binary mode.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input (unused)
static bool
option_b(struct options* opts, const char* inp)
{
  (void)inp;
  opts->op_bin = true;

  return true;
}

/// Run the process as a system daemon.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input (unused)
static bool
option_d(struct options* opts, const char* inp)
{
  (void)inp;
  opts->op_dmon = true;

  return true;
}

/// Terminate the process on first network-related error.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input (unused)
static bool
option_e(struct options* opts, const char* inp)
{
  (void)inp;
  opts->op_err = true;

  return true;
}

/// Print the usage help message and exit the process.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input (unused)
static bool
option_h(struct options* opts, const char* inp)
{
  (void)opts;
  (void)inp;

  print_usage();
  exit(EXIT_FAILURE);

  return true;
}

/// Set a unique key to identify the flow.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input
static bool
option_k(struct options* opts, const char* inp)
{
  return parse_uint64(&opts->op_key, inp, 1, UINT64_MAX);
}

/// Enable monologue mode where no responses are being issued.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input (unused)
static bool
option_m(struct options* opts, const char* inp)
{
  (void)inp;
  opts->op_mono = true;

  return true;
}

/// Turn off coloring and highlights in the logging output.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input (unused)
static bool
option_n(struct options* opts, const char* inp)
{
  (void)inp;
  opts->op_lcol = false;

  return true;
}

/// Set the UDP port number used for all communication.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input
static bool
option_p(struct options* opts, const char* inp)
{
  return parse_uint64(&opts->op_port, inp, 1, 65535);
}

/// 
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input (unused)
static bool
option_q(struct options* opts, const char* inp)
{
  (void)inp;
  opts->op_sil = true;

  return true;
}

/// Set the socket receive buffer size.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input
static bool
option_r(struct options* opts, const char* inp)
{
  return parse_scalar(&opts->op_rbuf, inp, "b", parse_memory_unit);
}

/// Set the socket send buffer size.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input
static bool
option_s(struct options* opts, const char* inp)
{
  return parse_scalar(&opts->op_sbuf, inp, "b", parse_memory_unit);
}

/// Set the IP Time-To-Live value.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input
static bool
option_t(struct options* opts, const char* inp)
{
  return parse_uint64(&opts->op_ttl, inp, 1, 255);
}

/// Increase the logging verbosity.
/// @return success/failure indication
///
/// @param[out] opts options
/// @param[in]  inp  argument input (unused)
static bool
option_v(struct options* opts, const char* inp)
{
  (void)inp;

  if (opts->op_llvl == LL_DEBUG) opts->op_llvl = LL_TRACE;
  if (opts->op_llvl == LL_INFO)  opts->op_llvl = LL_DEBUG;
  if (opts->op_llvl == LL_WARN)  opts->op_llvl = LL_INFO;
  if (opts->op_llvl == LL_ERROR) opts->op_llvl = LL_WARN;

  return true;
}

/// Generate a random key.
/// @return random 64-bit unsigned integer (non-zero)
static uint64_t
generate_key(void)
{
  uint64_t key;

  // Seed the psuedo-random number generator. The generated key is not intended
  // to be cryptographically safe - it is just intended to prevent publishers
  // from the same host to share the same key. The only situation that this
  // could still happen is if system PIDs loop over and cause the following
  // equation: t1 + pid1 == t2 + pid2, which was ruled to be unlikely.
  srand48(time(NULL) + getpid());

  // Generate a random key and ensure it is not a zero. The zero value
  // is internally used to represent the state where no filtering of keys
  // is performed by the subscriber process.
  do {
    key = (uint64_t)lrand48() | ((uint64_t)lrand48() << 32);
  } while (key == 0);

  return key;
}

/// Assign default values to all options.
///
/// @param[out] opts command-line options
static void
set_defaults(struct options* opts)
{
  intmax_t i;

  for (i = 0; i < PLUG_MAX; i++)
    opts->op_plgs[i] = NULL;
  opts->op_rbuf = DEF_RECEIVE_BUFFER_SIZE;
  opts->op_sbuf = DEF_SEND_BUFFER_SIZE;
  opts->op_err  = DEF_EXIT_ON_ERROR;
  opts->op_port = DEF_UDP_PORT;
  opts->op_ttl  = DEF_TIME_TO_LIVE;
  opts->op_mono = DEF_MONOLOGUE;
  opts->op_dmon = DEF_DAEMON;
  opts->op_sil  = DEF_SILENT;
  opts->op_bin  = DEF_BINARY;
  opts->op_llvl = (log_lvl = DEF_LOG_LEVEL);
  opts->op_lcol = (log_col = DEF_LOG_COLOR);
  opts->op_key  = generate_key();
  opts->op_ipv4 = false;
  opts->op_ipv6 = false;
}

/// Sanitize the IPv4/IPv6 options.
/// @return success/failure indication
///
/// @param[out] opts command-line options
static bool
organize_protocols(struct options* opts)
{
  // Check whether two exclusive modes were selected.
  if (opts->op_ipv4 == true && opts->op_ipv6 == true) {
    log(LL_WARN, false, "main", "options -4 and -6 are mutually exclusive");
    return false;
  }

  // If no restrictions on the IP version were set, enable both versions.
  if (opts->op_ipv4 == false && opts->op_ipv6 == false) {
    opts->op_ipv4 = true;
    opts->op_ipv6 = true;
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

/// Parse command-line options.
/// @return success/failure indication
///
/// @param[out] opts command-line options
/// @param[in]  argc argument count
/// @param[in]  argv argument vector
bool
parse_options(struct options* opts, int argc, char* argv[])
{
  int opt;
  bool retb;
  uint64_t i;
  char optdsl[128];
  struct option ents[16] = { 
    { '4',  false, option_4 },
    { '6',  false, option_6 },
    { 'a',  true , option_a },
    { 'b',  false, option_b },
    { 'd',  false, option_d },
    { 'e',  false, option_e },
    { 'h',  false, option_h },
    { 'k',  true , option_k },
    { 'm',  false, option_m },
    { 'n',  false, option_n },
    { 'p',  true , option_p },
    { 'q',  false, option_q },
    { 'r',  true , option_r },
    { 's',  true , option_s },
    { 't',  true , option_t },
    { 'v',  false, option_v } 
  };

  log(LL_INFO, false, "main", "parsing command-line options");

  (void)memset(optdsl, '\0', sizeof(optdsl));
  generate_getopt_string(optdsl, ents, 16);

  // Set optional arguments to sensible defaults.
  set_defaults(opts);

  // Loop through available options.
  while (true) {
    // Parse the next option.
    opt = getopt(argc, argv, optdsl);
    if (opt == -1)
      break;

    // Unknown option.
    if (opt == '?') {
      print_usage();
      log(LL_WARN, false, "main", "unknown option %c", optopt);

      return false;
    }

    // Find the relevant option.
    for (i = 0; i < 16; i++) {
      if (ents[i].op_name == (char)opt) {
        retb = ents[i].op_act(opts, optarg);
        if (retb == false) {
          log(LL_WARN, false, "main", "action for option '%c' failed", opt);
          return false;
        }

        break;
      }
    }
  }

  retb = organize_protocols(opts);
  if (retb == false)
    return false;

  // Assign the logging settings.
  log_lvl = opts->op_llvl;
  log_col = opts->op_lcol;

  return true;
}

/// Use the logging framework to output the selected option values used
/// throughout the run of the program.
void
log_options(const struct options* opts)
{
  log(LL_DEBUG, false, "main", "UDP port: %" PRIu64, opts->op_port);
  log(LL_DEBUG, false, "main", "unique key: %" PRIu64, opts->op_key);
  log(LL_DEBUG, false, "main", "Time-To-Live: %" PRIu64, opts->op_ttl);
  log(LL_DEBUG, false, "main", "receive buffer size: %" PRIu64 " bytes",
    opts->op_rbuf);
  log(LL_DEBUG, false, "main", "send buffer size: %" PRIu64 " bytes",
    opts->op_sbuf);
  log(LL_DEBUG, false, "main", "monologue mode: %s",
    opts->op_mono == true ? "on" : "off");
  log(LL_DEBUG, false, "main", "daemon process: %s",
    opts->op_dmon == true ? "yes" : "no");
  log(LL_DEBUG, false, "main", "binary report: %s",
    opts->op_dmon == true ? "on" : "off");
}
