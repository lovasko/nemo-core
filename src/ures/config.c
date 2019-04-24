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
#define DEF_BINARY              false

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
    "  -4      Use only the IPv4 protocol.\n"
    "  -6      Use only the IPv6 protocol.\n"
    "  -a OBJ  Attach a plugin from a shared object file.\n"
    "  -b      Reporting data to be in binary format.\n"
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
    NEMO_RES_VERSION_MAJOR,
    NEMO_RES_VERSION_MINOR,
    NEMO_RES_VERSION_PATCH,
    NEMO_PAYLOAD_VERSION,
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
/// @param[out] cf configuration
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

/// Generate a random key.
/// @return success/failure indication
///
/// @param[out] key random 64-bit unsigned integer
static bool 
generate_key(uint64_t* key)
{
  uint64_t data;
  ssize_t retss;
  int reti;
  int dr;

  // Prepare the source of random data.
  dr = open("/dev/urandom", O_RDONLY);
  if (dr == -1) {
    log(LL_WARN, true, "unable to open file %s", "/dev/random");
    return false;
  }

  // Obtain a random key.
  retss = read(dr, &data, sizeof(data));
  if (retss != (ssize_t)sizeof(data)) {
    log(LL_WARN, true, "unable to obtain random bytes");
     
    // Close the random source.
    reti = close(dr);
    if (reti == -1) {
      log(LL_WARN, true, "unable to close file %s", "/dev/random");
    }

    return false;
  }

  // Close the random source.
  reti = close(dr);
  if (reti == -1) {
    log(LL_WARN, true, "unable to close file %s", "/dev/random");
    return false;
  }

  *key = data;
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
  bool retb;

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
  cf->cf_bin  = DEF_BINARY;
  cf->cf_llvl = (log_lvl = DEF_LOG_LEVEL);
  cf->cf_lcol = (log_col = DEF_LOG_COLOR);
  cf->cf_ipv4 = false;
  cf->cf_ipv6 = false;

  retb = generate_key(&cf->cf_key);
  if (retb == false) {
    log(LL_WARN, false, "unable to generate a random key");
    return false;
  }

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
  struct option opts[15] = {
    { '4',  false, option_4 },
    { '6',  false, option_6 },
    { 'a',  true , option_a },
    { 'b',  false, option_b },
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
/// throughout the run of the program.
///
/// @param[in] cf configuration
void
log_config(const struct config* cf)
{
  log(LL_DEBUG, false, "UDP port: %" PRIu64, cf->cf_port);
  log(LL_DEBUG, false, "unique key: %" PRIu64, cf->cf_key);
  log(LL_DEBUG, false, "Time-To-Live: %" PRIu64, cf->cf_ttl);
  log(LL_DEBUG, false, "receive buffer size: %" PRIu64 " bytes", cf->cf_rbuf);
  log(LL_DEBUG, false, "send buffer size: %" PRIu64 " bytes", cf->cf_sbuf);
  log(LL_DEBUG, false, "monologue mode: %s", cf->cf_mono == true ? "yes" : "no");
  log(LL_DEBUG, false, "binary report: %s", cf->cf_bin == true ? "yes" : "no");
}
