// Copyright (c) 2018 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h>

#include "common/convert.h"
#include "common/daemon.h"
#include "common/log.h"
#include "common/parse.h"
#include "common/payload.h"
#include "ureq/version.h"


// Default values for optional arguments.
#define DEF_COUNT               5          // Number of published datagrams.
#define DEF_INTERVAL            1000000000 // One second pause between payloads.
#define DEF_WAIT_FOR_RESPONSES  2000000000 // Two second wait time for responses.
#define DEF_TIME_TO_LIVE        32         // IP Time-To-Live value.
#define DEF_EXIT_ON_ERROR       false      // Process exit on publishing error.
#define DEF_LOG_LEVEL           LL_WARN    // Log errors and warnings by default.
#define DEF_LOG_COLOR           true       // Colors in the notification output.
#define DEF_MONOLOGUE           false      // Responses are recorded by default.
#define DEF_DAEMON              false      // Process stays within the terminal.
#define DEF_UDP_PORT            23000      // UDP port number to use
#define DEF_RECEIVE_BUFFER_SIZE 2000000    // Socket receive buffer memory size.
#define DEF_SEND_BUFFER_SIZE    2000000    // Socket send buffer memory size.

// Target types.
#define NEMO_TARGET_TYPE_IPV4 1 ///< IP version 4.
#define NEMO_TARGET_TYPE_IPV6 2 ///< IP version 6.
#define NEMO_TARGET_TYPE_NAME 3 ///< Named address.

// Target limits.
#define NEMO_TARGET_NAME_MAX  63
#define NEMO_TARGET_COUNT_MAX 512

/// Network endpoint target.
typedef struct _target {
  uint8_t  tg_type;                        ///< One of NEMO_TARGET_TYPE*.
  char     tg_name[NEMO_TARGET_COUNT_MAX]; ///< Name service identifier.
  uint64_t tg_laddr;                       ///< Low bits of the address.
  uint64_t tg_haddr;                       ///< High bits of the address.
  uint64_t tg_time;                        ///< Time of last name resolution.
} target;

/// Arguments for a worker thread.
typedef struct _thread_arg {
  int       ta_socket;   ///< Network socket connection.
  char      ta_name[16]; ///< Name of the thread.
  pthread_t ta_friend;   ///< Related thread.
} thread_arg;

// Program options.
static uint64_t op_cnt;  ///< Number of emitted payload rounds.
static uint64_t op_int;  ///< Inter-payload sleep interval.
static uint64_t op_wait; ///< Wait time for responses after last request.
static uint64_t op_rbuf; ///< Socket receive buffer memory size.
static uint64_t op_sbuf; ///< Socket send buffer memory size.
static uint64_t op_ttl;  ///< Time-To-Live for published datagrams.
static uint64_t op_key;  ///< Key of the current process.
static uint64_t op_port; ///< UDP port for all endpoints.
static uint8_t  op_llvl; ///< Notification verbosity level.
static bool     op_lcol; ///< Notification coloring policy.
static bool     op_err;  ///< Process exit policy on publishing error.
static bool     op_mono; ///< Do not capture responses (monologue mode).
static bool     op_dmon; ///< Daemon process.
static bool     op_ipv4; ///< IPv4-only mode.
static bool     op_ipv6; ///< IPv6-only mode.

// Global state.
static int sock4;                         ///< IPv4 socket.
static int sock6;                         ///< IPv6 socket.
static volatile bool sint;                         ///< SIGINT flag.
static volatile bool sterm;                        ///< SIGTERM flag.
static volatile bool susr1;                        ///< SIGUSR1 flag.
static uint64_t ntgs;                     ///< Number of targets.
static target tgs[NEMO_TARGET_COUNT_MAX]; ///< Target database.
static pthread_t sender4;                 ///< IPv4 sender thread.
static pthread_t recver4;                 ///< IPv4 receiver thread.
static pthread_t sender6;                 ///< IPv6 sender thread.
static pthread_t recver6;                 ///< IPv6 receiver thread.
static pthread_t mainth;                  ///< Thread of the main function.


/// Send the SIGUSR1 signal to all other worker threads apart from the calling
/// one.
static void
kill_other_threads(void)
{
  pthread_t self;
  int reti;

  // Rationale: it is expected that the thread that calls this function will
  // exit on its own and therefore the SIGUSR1 signal should be only sent to
  // the rest of the operating threads.
  self = pthread_self();

  if (op_ipv4 == true) {
    reti = pthread_equal(self, sender4);
    if (reti == 0)
      (void)pthread_kill(sender4, SIGUSR1);

    reti = pthread_equal(self, recver4);
    if (reti == 0)
      (void)pthread_kill(recver4, SIGUSR1);
  }

  if (op_ipv6 == true) {
    reti = pthread_equal(self, sender6);
    if (reti == 0)
      (void)pthread_kill(sender6, SIGUSR1);

    reti = pthread_equal(self, recver6);
    if (reti == 0)
      (void)pthread_kill(recver6, SIGUSR1);
  }
}

/// Print the usage information to the standard output stream.
static void
print_usage(void)
{
  printf(
    "About:\n"
    "  Unicast network requester.\n"
    "  Program version: %d.%d.%d\n"
    "  Payload version: %d\n\n"

    "Usage:\n"
    "  nreq [-46dehmnv] [-c CNT] [-i DUR] [-k KEY] [-r RBS] [-s SBS]\n"
    "       [-p NUM] [-t TTL] [-w DUR] target [target]...\n\n"

    "Arguments:\n"
    "  target  IPv4/IPv6 address\n\n"

    "Options:\n"
    "  -4      Use only the IPv4 protocol.\n"
    "  -6      Use only the IPv6 protocol.\n"
    "  -c CNT  Number of requests to issue. (def=%d)\n"
    "  -d      Run the process as a daemon.\n"
    "  -e      Stop the process on first network error.\n"
    "  -h      Print this help message.\n"
    "  -i DUR  Interval duration between published datagram rounds. (def=1s)\n"
    "  -k KEY  Key for the current run. (def=random)\n"
    "  -m      Do not react to responses (monologue mode).\n"
    "  -n      Turn off colors in logging messages.\n"
    "  -r RBS  Receive memory buffer size.\n"
    "  -s SBS  Send memory buffer size.\n"
    "  -p NUM  UDP port to use for all endpoints. (def=%d)\n"
    "  -t TTL  Set the Time-To-Live for all published datagrams. (def=%d)\n"
    "  -v      Increase the verbosity of the logging output.\n"
    "  -w DUR  Wait time for responses after last request. (def=2s)\n",
    NEMO_REQ_VERSION_MAJOR,
    NEMO_REQ_VERSION_MINOR,
    NEMO_REQ_VERSION_PATCH,
    NEMO_PAYLOAD_VERSION,
    DEF_COUNT,
    DEF_UDP_PORT,
    DEF_TIME_TO_LIVE);
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

/// Parse the command-line options.
/// @return status code
///
/// @param[out] pidx  index where positional arguments start
/// @param[in]  argc argument count
/// @param[in]  argv argument vector
static bool
parse_options(int* pidx, int argc, char* argv[])
{
  int opt;
  bool retb;

  // Set optional arguments to sensible defaults.
  op_rbuf = DEF_RECEIVE_BUFFER_SIZE;
  op_sbuf = DEF_SEND_BUFFER_SIZE;
  op_cnt  = DEF_COUNT;
  op_int  = DEF_INTERVAL;
  op_wait = DEF_WAIT_FOR_RESPONSES;
  op_ttl  = DEF_TIME_TO_LIVE;
  op_err  = DEF_EXIT_ON_ERROR;
  op_mono = DEF_MONOLOGUE;
  op_dmon = DEF_DAEMON;
  op_port = DEF_UDP_PORT;
  op_llvl = (log_lvl = DEF_LOG_LEVEL);
  op_lcol = (log_col = DEF_LOG_COLOR);
  op_key  = generate_key();
  op_ipv4 = false;
  op_ipv6 = false;

  // Loop through available options.
  while (true) {
    // Parse the next available option.
    opt = getopt(argc, argv, "46c:dehi:k:mnp:r:s:t:vw:");
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

      // Number of requests to make.
      case 'c':
        retb = parse_uint64(&op_cnt, optarg, 1, UINT64_MAX);
        if (retb == false)
          return false;
        break;

      // Daemon process.
      case 'd':
        op_dmon = true;
        break;

      // Process exit on first network error.
      case 'e':
        op_err = true;
        break;

      // Usage information.
      case 'h':
        print_usage();
        _exit(EXIT_FAILURE);
        return false;

      // Time interval between rounds of sent datagrams.
      case 'i':
        retb = parse_scalar(&op_int, optarg, "ns", parse_time_unit);
        if (retb == false)
          return false;
        break;

      // Key of the current run.
      case 'k':
        retb = parse_uint64(&op_key, optarg, 1, UINT64_MAX);
        if (retb == false)
          return false;
        break;

      // Do not expect responses (monologue mode).
      case 'm':
        op_mono = true;
        break;

      // Turn of colors and highlight in the logging output.
      case 'n':
        op_lcol = false;
        break;

      // UDP port to use.
      case 'p':
        retb = parse_uint64(&op_port, optarg, 1, 65535);
        if (retb == false)
          return false;
        break;

      // Socket receive buffer size.
      case 'r':
        retb = parse_scalar(&op_rbuf, optarg, "b", parse_memory_unit);
        if (retb == false)
          return false;
        break;

      // Socket send buffer size.
      case 's':
        retb = parse_scalar(&op_sbuf, optarg, "b", parse_memory_unit);
        if (retb == false)
          return false;
        break;

      // Set the outgoing Time-To-Live value.
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

      // Wait time for responses after last request.
      case 'w':
        retb = parse_scalar(&op_wait, optarg, "ns", parse_time_unit);
        if (retb == false)
          return false;
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

  *pidx = optind;

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

/// Create a part of a IPv6 address by converting array of bytes into a
/// single integer.
/// @return status code
static uint64_t
make_address_part(const uint8_t* part)
{
  uint64_t res;
  uint8_t i;

  res = 0;
  for (i = 0; i < 8; i++)
    res |= ((uint64_t)part[i] << (i * 8));

  return res;
}

/// Parse network targets.
/// @return status code
///
/// @param[in] idx  index in the argument vector
/// @param[in] argc command-line argument count
/// @param[in] argv command-line argument vector
static bool
parse_targets(int idx, int argc, char* argv[])
{
  int i;
  int reti;
  struct in_addr addr4;
  struct in6_addr addr6;
  uint64_t tidx;

  tidx = 0;
  for (i = idx; i < argc; i++) {
    // Prepare the target structure.
    (void)memset(&tgs[tidx], 0, sizeof(tgs[tidx]));

    // Try parsing the address as IPv4.
    reti = inet_pton(AF_INET, argv[i], &addr4);
    if (reti == 1) {
      tgs[tidx].tg_type  = NEMO_TARGET_TYPE_IPV4;
      tgs[tidx].tg_laddr = (uint64_t)addr4.s_addr;
      tgs[tidx].tg_haddr = (uint64_t)0;

      log(LL_INFO, false, "main", "parsed %s target: %s", "IPv4", argv[i]);
      ntgs++;
      continue;
    }

    // Try parsing the address as IPv6.
    reti = inet_pton(AF_INET6, argv[i], &addr6);
    if (reti == 1) {
      tgs[tidx].tg_type  = NEMO_TARGET_TYPE_IPV6;
      tgs[tidx].tg_laddr = make_address_part(&addr6.s6_addr[0]);
      tgs[tidx].tg_haddr = make_address_part(&addr6.s6_addr[7]);

      log(LL_INFO, false, "main", "parsed %s target: %s", "IPv6", argv[i]);
      ntgs++;
      continue;
    }

    // As none of the two address families were applicable, report a problem.
    if (reti == -1) {
      log(LL_WARN, false, "main", "unable to parse target %s", argv[i]);
      return false;
    }
  }

  return true;
}

/// Signal handler for the SIGINT signal.
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
  if (sig == SIGUSR1) susr1 = true;
}

/// Install signal handlers.
/// @return status code
static bool
install_signal_handlers(void)
{
  struct sigaction sa;
  int reti;
  sigset_t ss;

  log(LL_INFO, false, "main", "installing signal handlers");

  // Reset the signal indicator.
  sint  = false;
	sterm = false;
  susr1 = false;

  // Create a set of blocked signals.
  (void)sigfillset(&ss);
  (void)sigdelset(&ss, SIGINT);
	(void)sigdelset(&ss, SIGTERM);
  (void)sigdelset(&ss, SIGUSR1);
  (void)sigprocmask(SIG_SETMASK, &ss, NULL);

  // Initialise the handler settings.
  (void)memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;

  // Install signal handler for SIGINT.
  reti = sigaction(SIGINT, &sa, NULL);
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to add signal handler for %s", "SIGINT");
    return false;
  }

  // Install signal handler for SIGTERM.
  reti = sigaction(SIGTERM, &sa, NULL);
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to add signal handler for %s", "SIGTERM");
    return false;
  }

  // Install signal handler for SIGUSR1.
  reti = sigaction(SIGUSR1, &sa, NULL);
  if (reti == -1) {
    log(LL_WARN, true, "main", "unable to add signal handler for %s", "SIGUSR1");
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
    log(LL_WARN, true, "main", "unable to set the %s socket address reusable", "IPv4");
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
    log(LL_WARN, true, "main", "unable to set the %s socket receive buffer size to %d",
         "IPv4", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)op_sbuf;
  ret = setsockopt(sock4, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket send buffer size to %d",\
         "IPv4", val);
    return false;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)op_ttl;
  ret = setsockopt(sock4, IPPROTO_IP, IP_TTL, &val, sizeof(val));
  if (ret == -1) {
    log(LL_WARN, true, "main", "unable to set the %s socket time-to-live to %d",
         "IPv4", val);
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

/// Convert the target address to a universal standard address type.
///
/// @param[out] ss universal address type
/// @param[in]  tg network target
static void
set_target_address(struct sockaddr_storage* ss, const target tg)
{
  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;

  if (tg.tg_type == NEMO_TARGET_TYPE_IPV4) {
    sin.sin_family      = AF_INET;
    sin.sin_port        = htons((uint16_t)op_port);
    sin.sin_addr.s_addr = (uint32_t)tg.tg_laddr;
    (void)memcpy(ss, &sin, sizeof(sin));
  }

  if (tg.tg_type == NEMO_TARGET_TYPE_IPV6) {
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port   = htons((uint16_t)op_port);
    tipv6(&sin6.sin6_addr, tg.tg_laddr, tg.tg_haddr);
    (void)memcpy(ss, &sin6, sizeof(sin6));
  }
}

/// Fill the payload with default and selected data.
///
/// @param[out] pl   payload
/// @param[in]  tg   network target
/// @param[in]  snum sequence number
/// @param[in]  rts  system time
/// @param[in]  mts  steady time
static void
fill_payload(struct payload *pl,
             const target* tg,
             const uint64_t snum,
             const struct timespec rts,
             const struct timespec mts)
{
  (void)memset(pl, 0, sizeof(*pl));
  pl->pl_mgic = NEMO_PAYLOAD_MAGIC;
  pl->pl_fver = NEMO_PAYLOAD_VERSION;
  pl->pl_type = NEMO_PAYLOAD_TYPE_REQUEST;
  pl->pl_port = (uint16_t)op_port;
  pl->pl_ttl1 = (uint8_t)op_ttl;
  pl->pl_snum = snum;
  pl->pl_slen = op_cnt;
  pl->pl_reqk = op_key;
  pl->pl_laddr = tg->tg_laddr;
  pl->pl_haddr = tg->tg_haddr;
  tnanos(&pl->pl_rtm1, rts);
  tnanos(&pl->pl_mtm1, mts);
}

/// Send a request to all selected targets.
/// @return status code
///
/// @param[in] sock socket
/// @param[in] snum sequence number
/// @param[in] name thread name
static bool
send_all_datagrams(int sock, const uint64_t snum, const char* name)
{
  uint64_t i;
  struct payload pl;
  struct timespec rts;
  struct timespec mts;
  struct msghdr msg;
  struct iovec data;
  struct sockaddr_storage addr;
  ssize_t n;

  // Prepare payload data.
  data.iov_base = &pl;
  data.iov_len  = sizeof(pl);

  // Prepare the message.
  msg.msg_name       = &addr;
  msg.msg_namelen    = sizeof(addr);
  msg.msg_iov        = &data;
  msg.msg_iovlen     = 1;
  msg.msg_control    = NULL;
  msg.msg_controllen = 0;

  for (i = 0; i < ntgs; i++) {
    // Assigning the correct address used by the target. This has to be called
    // before the encode_payload.
    set_target_address(&addr, tgs[i]);

    // Prepare payload.
    (void)clock_gettime(CLOCK_REALTIME,  &rts);
    (void)clock_gettime(CLOCK_MONOTONIC, &mts);
    fill_payload(&pl, &tgs[i], snum, rts, mts);
    pl.pl_pver = sock == sock4 ? 4 : 6;
    encode_payload(&pl);

    // Send the datagram.
    n = sendmsg(sock, &msg, MSG_DONTWAIT);
    if (n == -1) {
      log(LL_WARN, true, name, "unable to send datagram");

      if (op_err == false)
        return false;
    }

    // Verify the number of sent bytes.
    if ((size_t)n != sizeof(pl)) {
      log(LL_WARN, false, name, "unable to send all data");

      if (op_err == true)
        return false;
    }
  }

  return true;
}

/// Thread that sends requests to targets on a specified socket.
/// @return NULL
///
/// @param[in] arg socket file descriptor
static void*
send_worker(void* arg)
{
  uint64_t i;
  int reti;
  bool retb;
  struct timespec dur;
  thread_arg ta;

  // Resolve the thread argument.
  ta = *(thread_arg*)arg;
  log(LL_INFO, false, ta.ta_name, "starting thread");

  // Convert the interval duration.
  fnanos(&dur, op_int);

  // Start the sending loop.
  for (i = 0; i < op_cnt; i++) {

    log(LL_DEBUG, false, ta.ta_name, "sending datagram round %" PRIu64 "/%" PRIu64,
      i + 1, op_cnt);

    // Send all datagrams.
    retb = send_all_datagrams(ta.ta_socket, i, ta.ta_name);
    if (retb == false) {
      log(LL_WARN, false, ta.ta_name, "unable to send all payloads to IPv4 socket");

      if (op_err == true) {
        kill_other_threads();
        break;
      }
    }

    // Sleep for the desired interval.
    if (i != op_cnt - 1) {
      reti = nanosleep(&dur, NULL);
      if (reti == -1) {

        // Check if the sleep was interrupted by a signal.
        if (errno == EINTR) {
          if (sint == true) {
            log(LL_WARN, false, ta.ta_name, "received the %s signal", "SIGINT");
            kill_other_threads();
            break;
          }

          if (sterm == true) {
            log(LL_WARN, false, ta.ta_name, "received the %s signal", "SIGTERM");
            kill_other_threads();
            break;
          }

          if (susr1 == true)
            break;
        }

        log(LL_WARN, true, ta.ta_name, "unable to sleep");
      }
    }
  }

  // Final wait before terminating the receiver thread. This allows for the
  // remaining responses to arrive given a length trip time. The wait is not
  // performed in the monologue mode, as there are no expected responses to
  // wait for.
  if (op_mono == false && op_wait > 0
   && sint == false && sterm == false && susr1 == false) {
    log(LL_INFO, false, ta.ta_name, "waiting %" PRIu64 "%s for responses", op_wait, "ns");
    fnanos(&dur, op_wait);
    (void)nanosleep(&dur, NULL);
  }

  // Terminate the relevant receiver thread.
  (void)pthread_kill(ta.ta_friend, SIGUSR1);
  log(LL_INFO, false, ta.ta_name, "exiting thread");

  return NULL;
}

/// Thread that receives responses on a specified socket.
/// @return NULL
///
/// @param[in] arg socket file descriptor
static void*
recv_worker(void* arg)
{
  ssize_t retss;
  struct payload pl;
  struct msghdr msg;
  struct iovec iov;
  struct sockaddr_storage addr;
  thread_arg ta;

  // Short-circuit the receiver thread in the monologue mode.
  if (op_mono == true)
    return NULL;

  // Resolve the argument to a socket.
  ta = *(thread_arg*)arg;
  log(LL_INFO, false, ta.ta_name, "starting thread");

  // Prepare payload data.
  iov.iov_base = &pl;
  iov.iov_len  = sizeof(pl);

  // Prepare the message.
  msg.msg_name       = &addr;
  msg.msg_namelen    = sizeof(addr);
  msg.msg_iov        = &iov;
  msg.msg_iovlen     = 1;
  msg.msg_control    = NULL;
  msg.msg_controllen = 0;

  // Receive datagrams.
  while (true) {
    retss = recvmsg(ta.ta_socket, &msg, MSG_TRUNC);

    if (retss == -1) {
      // Check if we were interrupted by a signal delivery.
      if (errno == EINTR) {
				// If the signal was SIGINT or SIGTERM, send SIGUSR1 to all other
				// worker threads to interrupt their system calls.
        if (sint == true) {
				  log(LL_WARN, false, ta.ta_name, "received the %s signal", "SIGINT");
          kill_other_threads();
          break;
        }

        if (sterm == true) {
				  log(LL_WARN, false, ta.ta_name, "received the %s signal", "SIGTERM");
          kill_other_threads();
          break;
        }

        // Perform orderly exit after receiving the SIGUSR1 signal.
        if (susr1 == true)
          break;
      }

      log(LL_WARN, true, ta.ta_name, "unable to receive datagrams");
    }

    log(LL_DEBUG, false, ta.ta_name, "received message, responder key = %" PRIu64 "!", pl.pl_resk);
  }

  // Send a signal to the main thread so that it wakes up.
  (void)pthread_kill(ta.ta_friend, SIGUSR1);
  log(LL_INFO, false, ta.ta_name, "exiting thread");

  return NULL;
}

/// Initialise and start a thread of execution.
/// @return success/failure indication
///
/// @param[out] thread resulting thread
/// @param[in]  sock   network socket connection
/// @param[in]  friend related thread
/// @param[in]  name   name identifier
static bool
start_thread(pthread_t* thread,
             int sock,
             void*(*func)(void*),
             const char* name,
             pthread_t friend)
{
  int reti;
  thread_arg* ta;

  // Fill the thread arguments.
  ta = malloc(sizeof(*ta));
  ta->ta_socket = sock;
  ta->ta_friend = friend;
  (void)memset(ta->ta_name, '\0', sizeof(ta->ta_name));
  (void)strcpy(ta->ta_name, name);

  // Start the thread.
  reti = pthread_create(thread, NULL, func, ta);
  if (reti != 0) {
    errno = reti;
    log(LL_WARN, true, "main", "unable to start the %s thread", name);
    return false;
  }

  return true;
}

/// Start emitting the network requests.
/// @return status code
static bool
request_loop(void)
{
  bool retb;

  log(LL_INFO, false, "main", "starting the request loop");
  log_options();

  // Start the IPv4 sending/receiving.
  if (op_ipv4 == true) {
    retb = start_thread(&recver4, sock4, recv_worker, "rcv4", mainth);
    if (retb == false)
      return false;

    // The friend thread differs in the monologue mode, as once the sender
    // thread finishes, it needs to inform the main thread directly, waking it
    // up from the signal-induced sleep and perform the join operations.
    retb = start_thread(&sender4, sock4, send_worker, "snd4",
      op_mono == true ? mainth : recver4);
    if (retb == false)
      return false;
  }

  // Start the IPv6 sending/receiving.
  if (op_ipv6 == true) {
    retb = start_thread(&recver6, sock6, recv_worker, "rcv6", mainth);
    if (retb == false)
      return false;

    // The logic of selecting the friend thread is identical to the IPv4
    // reasoning above.
    retb = start_thread(&sender6, sock6, send_worker, "snd6",
      op_mono == true ? mainth : recver6);
    if (retb == false)
      return false;
  }

  // Wait for the delivery of the any signal. This can be either an externally
  // generated signal, or an internal death signal by either one of the
  // receiver threads. Since the delivery of the external signal can be
  // dispatched to any thread, we need to terminate all worker threads.  Once
  // we send the USR1 signal to all of them to interrupt the system calls they
  // are in, we wait for their orderly finish by joining them to the main
  // thread.

  while (true) {
    (void)pause();

    if (sint == true || sterm == true) {
      kill_other_threads();
    }

    if (susr1 == true) {
      if (op_ipv4 == true) {
        (void)pthread_join(sender4, NULL);
        (void)pthread_join(recver4, NULL);
      }

      if (op_ipv6 == true) {
        (void)pthread_join(sender6, NULL);
        (void)pthread_join(recver6, NULL);
      }

      log(LL_INFO, false, "main", "request loop has finished");
      break;
    }
  }

  // We only successfully exit if none of the SIGTERM and SIGINT signals
  // appeared.
  return (sterm == false && sint == false);
}

/// Unicast network requester.
int
main(int argc, char* argv[])
{
  bool retb;
  int pidx;

  // Obtain the main thread handle.
  mainth = pthread_self();

  // Parse command-line options.
  retb = parse_options(&pidx, argc, argv);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to parse command-line options");
    return EXIT_FAILURE;
  }

  // Optionally turn the process into a daemon.
  if (op_dmon == true) {
    retb = turn_into_daemon();
    if (retb == false) {
      log(LL_ERROR, false, "main", "unable to turn process into daemon");
      return EXIT_FAILURE;
    }
  }

  // Parse network targets.
  retb = parse_targets(pidx, argc, argv);
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to parse network targets");
    return EXIT_FAILURE;
  }

  // Install signal handlers.
  retb = install_signal_handlers();
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to install signal handlers");
    return EXIT_FAILURE;
  }

  // Create the IPv4 socket.
  retb = create_socket4();
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to create the IPv4 socket");
    return EXIT_FAILURE;
  }

  // Create the IPv6 socket.
  retb = create_socket6();
  if (retb == false) {
    log(LL_ERROR, false, "main", "unable to create the IPv6 socket");
    return EXIT_FAILURE;
  }

  // Start emitting requests.
  retb = request_loop();
  if (retb == false) {
    log(LL_ERROR, false, "main", "the request loop has ended unexpectedly");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}