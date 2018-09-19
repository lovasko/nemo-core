// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>
#include <fcntl.h>
#include <dlfcn.h>

#include "common/payload.h"
#include "common/version.h"
#include "util/convert.h"
#include "util/daemon.h"
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

// Command-line options.
static uint64_t op_port; ///< UDP port number.
static uint64_t op_rbuf; ///< Socket receive buffer size.
static uint64_t op_sbuf; ///< Socket send buffer size.
static uint64_t op_key;  ///< Unique key.
static uint64_t op_ttl;  ///< Time-To-Live for outgoing IP packets.
static bool     op_err;  ///< Early exit on first network error.
static bool     op_ipv4; ///< IPv4-only traffic.
static bool     op_ipv6; ///< IPv6-only traffic.
static uint8_t  op_llvl; ///< Minimal log level.
static bool     op_lcol; ///< Log coloring policy.
static bool     op_mono; ///< Monologue mode (no responses).
static bool     op_dmon; ///< Daemon process.

// Global state.
static int sock4;  ///< UDP/IPv4 socket.
static int sock6;  ///< UDP/IPv6 socket.
static bool sint;  ///< Signal interrupt indicator.
static bool sterm; ///< Signal termination indicator.

struct action {
  char* ac_name;
  void* ac_hndl;
  bool (*ac_init)(void);
  bool (*ac_evnt)(uint64_t, uint64_t, uint64_t, uint64_t);
  bool (*ac_free)(void);
};

#define NEMO_ACTION_MAX 32

static struct action acs[NEMO_ACTION_MAX]; ///< Actions.
static uint64_t nacs;                      ///< Number of actions.

/// Dynamically load the shared object and extract all necessary symbols that
/// define the action from it.
/// @return success/failure indication
///
/// @param[out] ac   action
/// @param[in]  path file path to the shared object
static bool
load_action(struct action* ac, const char* path)
{
  char estr[128];

  memset(estr, '\0', sizeof(estr));

  ac->ac_hndl = dlopen(path, RTLD_NOW);
  if (ac->ac_hndl == NULL) {
    strncpy(estr, "unable to open %s:", 64);
    strncat(estr, dlerror(), 64);
    log(LL_WARN, false, "main", estr, path);
    return false;
  }

  ac->ac_name = dlsym(ac->ac_hndl, "nemo_name");
  if (ac->ac_name == NULL) {
    strncpy(estr, "unable to load symbol %s:", 64);
    strncat(estr, dlerror(), 64);
    log(LL_WARN, false, "main", estr, "nemo_name");
    return false;
  }

  ac->ac_init = dlsym(ac->ac_hndl, "nemo_init");
  if (ac->ac_init == NULL) {
    strncpy(estr, "unable to load symbol %s:", 64);
    strncat(estr, dlerror(), 64);
    log(LL_WARN, false, "main", estr, "nemo_init");
    return false;
  }

  ac->ac_evnt = dlsym(ac->ac_hndl, "nemo_evnt");
  if (ac->ac_evnt == NULL) {
    strncpy(estr, "unable to load symbol %s:", 64);
    strncat(estr, dlerror(), 64);
    log(LL_WARN, false, "main", estr, "nemo_evnt");
    return false;
  }

  ac->ac_free = dlsym(ac->ac_hndl, "nemo_free");
  if (ac->ac_free == NULL) {
    strncpy(estr, "unable to load symbol %s:", 64);
    strncat(estr, dlerror(), 64);
    log(LL_WARN, false, "main", estr, "nemo_free");
    return false;
  }

  return true;
}

static bool
start_actions(void)
{
  uint64_t i;
  bool retb;

  for (i = 0; i < nacs; i++) {
    retb = acs[i].ac_init();
    if (retb == false)
      return false;
  }

  return true;
}

static bool
trigger_actions(const payload* pl)
{
  uint64_t i;
  bool retb;

  for (i = 0; i < nacs; i++) {
    retb = acs[i].ac_evnt(pl->pl_resk, pl->pl_reqk, pl->pl_reqk, pl->pl_reqk);
    if (retb == false)
      return false;
  }

  return true;
}

static bool
terminate_actions(void)
{
  uint64_t i;
  bool retb;

  for (i = 0; i < nacs; i++) {
    retb = acs[i].ac_free();
    if (retb == false)
      return false;
  }

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
    "  nres [-46dehmnv] [-a OBJ] [-k KEY] [-p NUM] [-r RBS] [-s SBS] "
    "[-t TTL]\n\n"

    "Options:\n"
    "  -a OBJ  Path to shared object file of an action set.\n"
    "  -4      Use only the IPv4 protocol.\n"
    "  -6      Use only the IPv6 protocol.\n"
    "  -d      Run the process as a daemon.\n"
    "  -e      Stop the process on first transmission error.\n"
    "  -h      Print this help message.\n"
    "  -k KEY  Key for the current run. (def=random)\n"
    "  -m      Disable responding (monologue mode).\n"
    "  -n      Turn off coloring in the logging output.\n"
    "  -p NUM  UDP port to use for all endpoints. (def=%d)\n"
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

/// Parse command-line options.
/// @return success/failure indication
///
/// @param[in] argc argument count
/// @param[in] argv argument vector
static bool
parse_options(int argc, char* argv[])
{
  int opt;
  bool retb;

  log(LL_INFO, false, "main", "parse command-line options");

  // Set optional arguments to sensible defaults.
  op_rbuf = DEF_RECEIVE_BUFFER_SIZE;
  op_sbuf = DEF_SEND_BUFFER_SIZE;
  op_err  = DEF_EXIT_ON_ERROR;
  op_port = DEF_UDP_PORT;
  op_ttl  = DEF_TIME_TO_LIVE;
  op_mono = DEF_MONOLOGUE;
  op_dmon = DEF_DAEMON;
  op_llvl = (log_lvl = DEF_LOG_LEVEL);
  op_lcol = (log_col = DEF_LOG_COLOR);
  op_key  = generate_key();
  op_ipv4 = false;
  op_ipv6 = false;
  nacs    = 0;

  // Loop through available options.
  while (true) {
    // Parse the next option.
    opt = getopt(argc, argv, "46dehk:mnp:r:s:t:v");
    if (opt == -1)
      break;

    switch (opt) {
      // IPv4-only mode.
      case '4':
        op_ipv4 = true;
        break;

      // IPv6-only mode.
      case '6':
        op_ipv6 = true;
        break;

      // Action from a shared object file.
      case 'a':
        retb = load_action(&acs[nacs], optarg);
        if (retb == false) {
          log(LL_WARN, false, "main", "unable to load action");
          return false;
        }
        nacs++;
        break;

      // Daemon process.
      case 'd':
        op_dmon = true;
        break;

      // Process exit on transmission error.
      case 'e':
        op_err = true;
        break;

      // Usage information.
      case 'h':
        print_usage();
        _exit(EXIT_FAILURE);
        return true;

      // Key of the current run.
      case 'k':
        retb = parse_uint64(&op_key, optarg, 1, UINT64_MAX);
        if (retb == false)
          return false;
        break;

      // Disable responding to payloads (monologue mode).
      case 'm':
        op_mono = true;
        break;

      // Disable coloring and highlighting of logging output.
      case 'n':
        op_lcol = false;
        break;

      // UDP port to use.
      case 'p':
        retb = parse_uint64(&op_port, optarg, 1, 65535);
        if (retb == false)
          return false;
        break;

      // Receive buffer memory size.
      case 'r':
        retb = parse_scalar(&op_rbuf, optarg, "b", parse_memory_unit);
        if (retb == false)
          return false;
        break;

      // Send buffer memory size.
      case 's':
        retb = parse_scalar(&op_sbuf, optarg, "b", parse_memory_unit);
        if (retb == false)
          return false;
        break;

      // Set IP Time-To-Live value.
      case 't':
        retb = parse_uint64(&op_ttl, optarg, 1, 255);
        if (retb == false)
          return false;
        break;

      // Increase the logging verbosity.
      case 'v':
        if (op_llvl == LL_DEBUG) op_llvl = LL_TRACE;
        if (op_llvl == LL_INFO)  op_llvl = LL_DEBUG;
        if (op_llvl == LL_WARN)  op_llvl = LL_INFO;
        if (op_llvl == LL_ERROR) op_llvl = LL_WARN;
        break;

      // Unknown option.
      case '?':
        print_usage();
        log(LL_WARN, false, "main", "unknown option %c", optopt);
        return false;

      // Unknown situation.
      default:
        print_usage();
        return false;
    }
  }

  // Check whether two exclusive modes were selected.
  if (op_ipv4 == true && op_ipv6 == true) {
    log(LL_WARN, false, "main", "options -4 and -6 are mutually exclusive");
    return false;
  }

  // If no restrictions on the IP version were set, enable both versions.
  if (op_ipv4 == false && op_ipv6 == false) {
    op_ipv4 = true;
    op_ipv6 = true;
  }

  // Assign the logging settings.
  log_lvl = op_llvl;
  log_col = op_lcol;

  return true;
}

/// Use the logging framework to output the selected option values used
/// throughout the run of the program.
static void
log_options(void)
{
  log(LL_DEBUG, false, "main", "selected UDP port: %" PRIu64, op_port);
  log(LL_DEBUG, false, "main", "selected unique key: %" PRIu64, op_key);
  log(LL_DEBUG, false, "main", "selected Time-To-Live: %" PRIu64, op_ttl);
  log(LL_DEBUG, false, "main", "selected receive buffer size: %"
    PRIu64 " bytes", op_rbuf);
  log(LL_DEBUG, false, "main", "selected send buffer size: %" PRIu64 " bytes",
    op_sbuf);
  log(LL_DEBUG, false, "main", "selected monologue mode: %s",
    op_mono == true ? "on" : "off");
}

/// Signal handler for the SIGINT and SIGTERM signals.
///
/// This handler does not perform any action, just toggles the indicator
/// for the signal. The actual signal handling is done by the respond_loop
/// function.
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
static bool
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

/// Create the IPv4 socket.
/// @return success/failure indication
static bool
create_socket4(void)
{
  int ret;
  struct sockaddr_in addr;
  int val;

  // Early exit if IPv4 is not selected.
  if (op_ipv4 == false)
    return true;

  log(LL_INFO, false, "main", "creating %s socket", "IPv4");

  // Create a UDP socket.
  sock4 = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock4 < 0) {
    log(LL_WARN, true, "main", "unable to initialise the %s socket", "IPv4");
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  ret = setsockopt(sock4, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket address reusable",
        "IPv4");
    return false;
  }

  // Bind the socket to the selected port and local address.
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons((uint16_t)op_port);
  addr.sin_addr.s_addr = INADDR_ANY;
  ret = bind(sock4, (struct sockaddr*)&addr, sizeof(addr));
  if (ret < 0) {
    log(LL_WARN, true, "main", "unable to bind the %s socket", "IPv4");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)op_rbuf;
  ret = setsockopt(sock4, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket receive buffer "
      "size to %d", "IPv4", val);
    return false;
  } // Set the socket send buffer size.
  val = (int)op_sbuf;
  ret = setsockopt(sock4, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket send buffer size "
      "to %d", "IPv4", val);
    return false;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)op_ttl;
  ret = setsockopt(sock4, IPPROTO_IP, IP_TTL, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket time-to-live to "
      "%d", "IPv4", val);
    return false;
  }

  return true;
}

/// Create the IPv6 socket.
/// @return success/failure indication
static bool
create_socket6(void)
{
  int ret;
  struct sockaddr_in6 addr;
  int val;

  // Early exit if IPv6 is not selected.
  if (op_ipv6 == false)
    return true;

  log(LL_INFO, false, "main", "creating %s socket", "IPv6");

  // Create a UDP socket.
  sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sock6 < 0) {
    log(LL_WARN, true, "main", "unable to initialize the %s socket", "IPv6");
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  ret = setsockopt(sock6, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket address reusable", "IPv6");
    return false;
  }

  // Set the socket to only receive IPv6 traffic.
  val = 1;
  ret = setsockopt(sock6, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to disable IPv4 traffic on %s socket", "IPv6");
    return false;
  }

  // Bind the socket to the selected port and local address.
  addr.sin6_family = AF_INET6;
  addr.sin6_port   = htons((uint16_t)op_port);
  addr.sin6_addr   = in6addr_any;
  ret = bind(sock6, (struct sockaddr*)&addr, sizeof(addr));
  if (ret < 0) {
    log(LL_WARN, true, "main", "unable to bind the %s socket", "IPv6");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)op_rbuf;
  ret = setsockopt(sock6, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to set the read socket buffer size to %d", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)op_sbuf;
  ret = setsockopt(sock6, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to set the socket send buffer size to %d", val);
    return false;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)op_ttl;
  ret = setsockopt(sock6, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to set time-to-live to %d", val);
    return false;
  }

  return true;
}

/// Verify the incoming payload for correctness.
/// @return success/failure indication
///
/// @param[in] n  length of the received data
/// @param[in] pl payload
static bool
verify_payload(const ssize_t n, const payload* pl)
{
  log(LL_TRACE, false, "main", "verifying payload");

  // Verify the size of the datagram.
  if ((size_t)n != sizeof(*pl)) {
    log(LL_WARN, false, "main", "wrong datagram size, expected: %zd, actual: %zu",
        n, sizeof(*pl));
    return false;
  }

  // Verify the magic identifier.
  if (pl->pl_mgic != NEMO_PAYLOAD_MAGIC) {
    log(LL_DEBUG, false, "main", "payload identifier unknown, expected: %"
        PRIx32 ", actual: %" PRIx32, NEMO_PAYLOAD_MAGIC, pl->pl_mgic);
    return false;
  }

  // Verify the payload version.
  if (pl->pl_fver != NEMO_PAYLOAD_VERSION) {
    log(LL_DEBUG, false, "main", "unsupported payload version, expected: %"
        PRIu8 ", actual: %" PRIu8, NEMO_PAYLOAD_VERSION, pl->pl_fver);
    return false;
  }

  // Verify the payload type.
  if (pl->pl_type != NEMO_PAYLOAD_TYPE_REQUEST) {
    log(LL_DEBUG, false, "main", "unexpected payload type, expected: %"
        PRIu8 ", actual: %" PRIu8, NEMO_PAYLOAD_TYPE_REQUEST, pl->pl_type);
    return false;
  }

  return true;
}

/// Receive datagrams on both IPv4 and IPv6.
/// @return success/failure indication
///
/// @param[out] addr IPv4/IPv6 address
/// @param[out] pl   payload
/// @param[in]  sock socket
static bool
receive_datagram(struct sockaddr_storage* addr, payload* pl, int sock)
{
  struct msghdr msg;
  struct iovec data;
  ssize_t n;
  bool retb;

  log(LL_TRACE, false, "main", "receiving datagram on %s socket",
      sock == sock4 ? "IPv4" : "IPv6");

  // Prepare payload data.
  data.iov_base = pl;
  data.iov_len  = sizeof(*pl);

  // Prepare the message.
  msg.msg_name       = addr;
  msg.msg_namelen    = sizeof(*addr);
  msg.msg_iov        = &data;
  msg.msg_iovlen     = 1;
  msg.msg_control    = NULL;
  msg.msg_controllen = 0;

  // Receive the message and handle potential errors.
  n = recvmsg(sock, &msg, MSG_DONTWAIT | MSG_TRUNC);
  if (n < 0) {
    log(LL_WARN, true, "main", "receiving has failed");

    if (op_err == true)
      return false;
  }

  // Convert the payload from its on-wire format.
  decode_payload(pl);

  // Verify the payload correctness.
  retb = verify_payload(n, pl);
  if (retb == false) {
    log(LL_WARN, false, "main", "invalid payload content");
    return false;
  }

  return true;
}

/// Update payload with local diagnostic information.
/// @return success/failure indication
///
/// @param[out] pl payload
static bool
update_payload(payload* pl)
{
  struct timespec mts;
  struct timespec rts;
  int ret;

  log(LL_TRACE, false, "main", "updating payload");

  // Change the message type.
  pl->pl_type = NEMO_PAYLOAD_TYPE_RESPONSE;

  // Sign the payload.
  pl->pl_resk = op_key;

  // Obtain the steady (monotonic) clock time.
  ret = clock_gettime(CLOCK_MONOTONIC, &mts);
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to obtain the steady time");
    return false;
  }
  tnanos(&pl->pl_mtm2, mts);

  // Obtain the system (real-time) clock time.
  ret = clock_gettime(CLOCK_REALTIME, &rts);
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to obtain the system time");
    return false;
  }
  tnanos(&pl->pl_rtm2, rts);

  return true;
}

/// Send a payload to the defined address.
/// @return success/failure indication
///
/// @param[in] sock socket
/// @param[in] pl   payload
/// @param[in] addr IPv4/IPv6 address
static bool
send_datagram(int sock, payload* pl, struct sockaddr_storage* addr)
{
  ssize_t n;
  struct msghdr msg;
  struct iovec data;

  log(LL_TRACE, false, "main", "sending datagram");

  // Prepare payload data.
  data.iov_base = pl;
  data.iov_len  = sizeof(*pl);

  // Prepare the message.
  msg.msg_name       = addr;
  msg.msg_namelen    = sizeof(*addr);
  msg.msg_iov        = &data;
  msg.msg_iovlen     = 1;
  msg.msg_control    = NULL;
  msg.msg_controllen = 0;

  // Convert the payload to its on-wire format.
  encode_payload(pl);

  // Send the datagram.
  n = sendmsg(sock, &msg, MSG_DONTWAIT);
  if (n < 0) {
    log(LL_WARN, true, "main", "unable to send datagram");

    if (op_err == true)
      return false;
  }

  // Verify the size of the sent datagram.
  if ((size_t)n != sizeof(*pl)) {
    log(LL_WARN, false, "main", "wrong sent payload size");

    if (op_err == true)
      return false;
  }

  return true;
}

/// Handle the event of an incoming datagram.
/// @return success/failure indication
///
/// @param[in] sock socket
/// @param[in] ipv  IP version
static bool
handle_event(int sock, const char* ipv)
{
  bool retb;
  struct sockaddr_storage addr;
  payload pl;

  log(LL_DEBUG, false, "main", "handling event on %s socket",
    sock == sock4 ? "IPv4" : "IPv6");

  // Receive a request.
  retb = receive_datagram(&addr, &pl, sock);
  if (retb == false) {
    log(LL_WARN, false, "main", "unable to receive datagram on the %s socket", ipv);

    // We do not continue with the response, given the receiving of the
    // request has failed. If the op_err option was selected, all network
    // transmission errors should be treated as fatal. In order to propagate
    // error, we need to return 'false', if op_err was 'true'. The same applies
    // in the opposite case.
    return !op_err;
  }

  // Update payload.
  retb = update_payload(&pl);
  if (retb == false) {
    log(LL_WARN, false, "main", "unable to update the payload");
    return false;
  }

  // Trigger all associated action sets.
  retb = trigger_actions(&pl);
  if (retb == false) {
    log(LL_WARN, false, "main", "unable to trigger all actions");
    return false;
  }

  // Do not respond if the monologue mode is turned on.
  if (op_mono == true)
    return true;

  // Send a response back.
  retb = send_datagram(sock4, &pl, &addr);
  if (retb == false) {
    log(LL_WARN, false, "main", "unable to send datagram on the %s socket", ipv);

    // Following the same logic as above in the receive stage.
    return !op_err;
  }

  return true;
}

/// Start responding to requests on both IPv4 and IPv6 sockets.
/// @return success/failure indication
static bool
respond_loop(void)
{
  int reti;
  int ndfs;
  bool retb;
  fd_set rfd;

  log(LL_INFO, false, "main", "starting the response loop");
  log_options();

  // Add sockets to the event list.
  FD_ZERO(&rfd);
  if (op_ipv4 == true) FD_SET(sock4, &rfd);
  if (op_ipv6 == true) FD_SET(sock6, &rfd);

  // Compute the file descriptor count.
  ndfs = (op_ipv4 == true && op_ipv6 == true) ? 5 : 4;

  while (true) {
    // Wait for incoming datagram events.
    reti = pselect(ndfs, &rfd, NULL, NULL, NULL, NULL);
    if (reti == -1) {
      // Check for interrupt (possibly due to a signal).
      if (errno == EINTR) {
        if (sint == true)
          log(LL_WARN, false, "main", "received the %s signal", "SIGINT");
        else if (sterm == true)
          log(LL_WARN, false, "main", "received the %s signal", "SIGTERM");
        else
          log(LL_WARN, false, "main", "unknown event queue interrupt");

        return false;
      }

      log(LL_WARN, true, "main", "waiting for events failed");
      return false;
    }

    // Handle incoming IPv4 datagrams.
    reti = FD_ISSET(sock4, &rfd);
    if (reti > 0) {
      retb = handle_event(sock4, "IPv4");
      if (retb == false)
        return false;
    }

    // Handle incoming IPv6 datagrams.
    reti = FD_ISSET(sock6, &rfd);
    if (reti > 0) {
      retb = handle_event(sock6, "IPv6");
      if (retb == false)
        return false;
    }
  }

  return true;
}

/// Unicast network responder.
int
main(int argc, char* argv[])
{
  bool retb;

  // Parse command-line options.
  retb = parse_options(argc, argv);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to parse command-line options");
    return EXIT_FAILURE;
  }

  // Optionally turn the process into a daemon.
  if (op_dmon == true) {
    retb = turn_into_daemon();
    if (retb == false) {
      log(LL_ERROR, false, "main", "unable to turn process into a daemon");
      return EXIT_FAILURE;
    }
  }

  // Install the signal handlers.
  retb = install_signal_handlers();
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to install signal handlers");
    return EXIT_FAILURE;
  }

  // Create the IPv4 socket.
  retb = create_socket4();
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to create %s socket", "IPv4");
    return EXIT_FAILURE;
  }

  // Create the IPv6 socket.
  retb = create_socket6();
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to create %s socket", "IPv6");
    return EXIT_FAILURE;
  }

  // Start actions.
  retb = start_actions();
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to start all actions");
    return EXIT_FAILURE;
  }

  // Start the main responding loop.
  retb = respond_loop();
  if (retb == false) {
    log(LL_ERROR, false, "main", "responding loop has finished");
    return EXIT_FAILURE;
  }

  // Terminate actions.
  retb = terminate_actions();
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to terminate all actions");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
