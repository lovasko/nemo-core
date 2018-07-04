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

#include "common/version.h"
#include "common/payload.h"
#include "util/convert.h"
#include "util/log.h"
#include "util/parse.h"


// Default values for optional arguments.
#define DEF_COUNT               5          // Number of published datagrams.
#define DEF_INTERVAL            1000000000 // One second pause between payloads.
#define DEF_TIME_TO_LIVE        32         // IP Time-To-Live value.
#define DEF_EXIT_ON_ERROR       false      // Process exit on publishing error.
#define DEF_LOG_LEVEL           1          // Log errors and warnings by default.
#define DEF_LOG_COLOR           true       // Colors in the notification output.
#define DEF_UDP_PORT            23000      // UDP 
#define DEF_RECEIVE_BUFFER_SIZE 2000000    // Socket receive buffer memory size.
#define DEF_SEND_BUFFER_SIZE    2000000    // Socket send buffer memory size.

// Target types.
#define NEMO_TARGET_TYPE_IPV4 1 ///< IP version 4.
#define NEMO_TARGET_TYPE_IPV6 2 ///< IP version 6.
#define NEMO_TARGET_TYPE_NAME 3 ///< Named address.

// Target limits.
#define NEMO_TARGET_NAME_MAX  64
#define NEMO_TARGET_COUNT_MAX 512

/// Network endpoint target.
typedef struct _target {
  uint8_t  tg_type;                        ///< Type of the target (one of NEMO_TARGET_*).
  char     tg_name[NEMO_TARGET_COUNT_MAX]; ///< Name used to describe 
  uint64_t tg_laddr;                       ///< Low bits of the address.
  uint64_t tg_haddr;                       ///< High bits of the address.
  uint64_t tg_time;                        ///< Time of last name resolution.
} target;

// Program options.
static uint64_t op_cnt;  ///< Number of emitted payload rounds.
static uint64_t op_int;  ///< Inter-payload sleep interval.
static uint64_t op_rbuf; ///< Socket receive buffer memory size.
static uint64_t op_sbuf; ///< Socket send buffer memory size.
static uint64_t op_ttl;  ///< Time-To-Live for published datagrams.
static uint64_t op_key;  ///< Key of the current process.
static uint64_t op_port; ///< UDP port for all endpoints.
static uint8_t  op_llvl; ///< Notification verbosity level.
static bool     op_lcol; ///< Notification coloring policy.
static bool     op_err;  ///< Process exit policy on publishing error.
static bool     op_ipv4; ///< IPv4-only mode.
static bool     op_ipv6; ///< IPv6-only mode.

// Global state.
static int sock4;                         ///< IPv4 socket.
static int sock6;                         ///< IPv6 socket.
static bool sint;                         ///< SIGINT flag.
static uint64_t ntgs;                     ///< Number of targets.
static target tgs[NEMO_TARGET_COUNT_MAX]; ///< Target database.

static void
print_usage(void)
{
  printf(
    "About:\n"
    "  Unicast network requester.\n"
    "  Program version: %d.%d.%d\n"
    "  Payload version: %d\n\n"

    "Usage:\n"
    "  nreq [OPTIONS] target [target]...\n\n"

    "Arguments:\n"
    "  target - IPv4/IPv6 address\n\n"

    "Options:\n"
    "  -4      Use only the IPv4 protocol.\n"
    "  -6      Use only the IPv6 protocol.\n"
    "  -c CNT  Number of requests to issue. (def=%d)\n"
    "  -e      Stop the process on first network error.\n"
    "  -h      Print this help message.\n"
    "  -i DUR  Interval duration between published datagram rounds. (def=1s)\n"
    "  -k KEY  Key for the current run. (def=random)\n"
    "  -n      Turn off colors in logging messages.\n"
    "  -r RBS  Send buffer size in bytes.\n"
    "  -s SBS  Send buffer size in bytes.\n"
    "  -p NUM  UDP port to use for all endpoints. (def=%d)\n"
    "  -t TTL  Set the Time-To-Live for all published datagrams. (def=%d)\n"
    "  -v      Increase the verbosity of the logging output.\n",
    NEMO_VERSION_MAJOR,
    NEMO_VERSION_MINOR,
    NEMO_VERSION_PATCH,
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

/// Parse the command-line arguments.
/// @return status code
///
/// @param[in] argc argument count
/// @param[in] argv argument vector
static bool
parse_arguments(int argc, char* argv[])
{
  int opt;

  // Set optional arguments to sensible defaults.
  op_rbuf = DEF_RECEIVE_BUFFER_SIZE;
  op_sbuf = DEF_SEND_BUFFER_SIZE;
  op_cnt  = DEF_COUNT;
  op_int  = DEF_INTERVAL;
  op_ttl  = DEF_TIME_TO_LIVE;
  op_err  = DEF_EXIT_ON_ERROR;
  op_port = DEF_UDP_PORT;
  op_llvl = (log_lvl = DEF_LOG_LEVEL);
  op_lcol = (log_col = DEF_LOG_COLOR);
  op_key  = generate_key();
  op_ipv4 = false;
  op_ipv6 = false;

  // Loop through available options.
  while ((opt = getopt(argc, argv, "46c:ehk:np:r:s:t:v")) != -1) {
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
        if (parse_uint64(&op_cnt, optarg, 1, UINT64_MAX) == false)
          return false;
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

      // Key of the current run.
      case 'k':
        if (parse_uint64(&op_key, optarg, 1, UINT64_MAX) == 0)
          return false;
        break;

      // UDP port to use.
      case 'p':
	if (parse_uint64(&op_port, optarg, 1, 65535) == false)
	  return false;
	break;

      // Socket receive buffer size.
      case 'r':
	if (parse_scalar(&op_rbuf, optarg, "b", parse_memory_unit) == false)
	  return false;
	break;

      // Socket send buffer size.
      case 's':
	if (parse_scalar(&op_sbuf, optarg, "b", parse_memory_unit) == false)
	  return false;
	break;

      // Set the outgoing Time-To-Live value.
      case 't':
	if (parse_unit64(&op_ttl, optarg, 1, 255) == false)
	  return false;
	break;

      // Increase the logging verbosity.
      case 'v':
	if (op_llvl != LL_DEBUG)
	  op_llvl++;
	break;

      // Unknown option.
      case '?':
        print_usage();
        log_(LL_WARN, false, "unknown option %c", optopt);
        return false;

      // Unknown situation.
      default:
        print_usage();
        return false;
    }
  }

  // Check whether two exclusive modes were selected.
  if (op_ipv4 && op_ipv6) {
    log_(LL_WARN, false, "options -4 and -6 are mutually exclusive");
    return false;
  }

  // If no restrictions on the IP version were set, enable both versions.
  if (!op_ipv4 && !op_ipv6) {
    op_ipv4 = true;
    op_ipv6 = true;
  }

  return true;
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
    res |= (part[i] << (i * 8));

  return res;
}

/// Parse network targets.
/// @return status code
///
/// @param[in] idx index in the argument vector
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
      tgs[tidx].tg_type  = NEMO_TARGET_IPV4;
      tgs[tidx].tg_laddr = (uint64_t)addr4.s_addr;
      tgs[tidx].tg_haddr = (uint64_t)0;
      continue;
    }
    
    // Try parsing the address as IPv6.
    reti = inet_pton(AF_INET6, argv[i], &addr6);
    if (reti == 1) {
      tgs[tidx].tg_type  = NEMO_TARGET_IPV6;
      tgs[tidx].tg_laddr = make_address_part(&addr6.s6_addr[0]);
      tgs[tidx].tg_haddr = make_address_part(&addr6.s6_addr[7]);
      continue;
    }

    // As none of the two address families were applicable, report a problem.
    if (reti == -1) {
      log_(LL_WARN, false, "unable to parse target %s", argv[i]);
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
  if (sig == SIGINT)
    sint = true;
}

/// Install signal handlers.
/// @return status code
static bool
install_signal_handler(void)
{
  struct sigaction sa;
  int reti;

  // Reset the signal indicator.
  sint = false;

  // Initialise the handler settings.
  (void)memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;

  // Install signal handler for SIGINT.
  reti = sigaction(SIGINT, &sa, NULL);
  if (reti == -1) {
    log_(NL_WARN, true, "unable to add signal handler for %s", "SIGINT");
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
  if (!op_ipv4)
    return true;

  log_(LL_INFO, false, "creating %s socket", "IPv4");
  
  // Create a UDP socket.
  sock4 = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock4 < 0) {
    log_(LL_WARN, true, "unable to initialise the %s socket", "IPv4"); 
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  ret = setsockopt(sock4, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (ret == -1) {
    log_(LL_WARN, true, "unable to set the %s socket address reusable", "IPv4");
    return false;
  }

  // Bind the socket to the selected port and local address.
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons((uint16_t)op_port);
  addr.sin_addr.s_addr = INADDR_ANY;
  ret = bind(sock4, (struct sockaddr*)&addr, sizeof(addr));
  if (ret < 0) {
    log_(LL_WARN, true, "unable to bind the %s socket", "IPv4");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)op_rbuf;
  ret = setsockopt(sock4, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (ret == -1) {
    log_(LL_WARN, true, "unable to set the %s socket receive buffer size to %d",
         "IPv4", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)op_sbuf;
  ret = setsockopt(sock4, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (ret == -1) {
    log_(LL_WARN, true, "unable to set the %s socket send buffer size to %d",\
         "IPv4", val);
    return false;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)op_ttl;
  ret = setsockopt(sock4, IPPROTO_IP, IP_TTL, &val, sizeof(val));
  if (ret == -1) {
    log_(LL_WARN, true, "unable to set the %s socket time-to-live to %d",
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
  if (!op_ipv6)
    return true;

  log_(LL_INFO, false, "creating %s socket", "IPv6");
  
  // Create a UDP socket.
  sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sock6 < 0) {
    log_(LL_WARN, true, "unable to initialize the %s socket", "IPv6"); 
    return false;
  }

  // Set the socket binding to be re-usable.
  val = 1;
  ret = setsockopt(sock6, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (ret == -1) {
    log_(LL_WARN, true, "unable to set the %s socket address reusable", "IPv6");
    return false;
  }

  // Set the socket to only receive IPv6 traffic.
  val = 1;
  ret = setsockopt(sock6, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
  if (ret == -1) {
    log_(LL_WARN, true, "unable to disable IPv4 traffic on %s socket", "IPv6");
    return false;
  }

  // Bind the socket to the selected port and local address.
  addr.sin6_family = AF_INET6;
  addr.sin6_port   = htons((uint16_t)op_port);
  addr.sin6_addr   = in6addr_any;
  ret = bind(sock6, (struct sockaddr*)&addr, sizeof(addr));
  if (ret < 0) {
    log_(LL_WARN, true, "unable to bind the %s socket", "IPv6");
    return false;
  }

  // Set the socket receive buffer size.
  val = (int)op_rbuf;
  ret = setsockopt(sock6, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  if (ret == -1) {
    log_(LL_WARN, true, "unable to set the read socket buffer size to %d", val);
    return false;
  }

  // Set the socket send buffer size.
  val = (int)op_sbuf;
  ret = setsockopt(sock6, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (ret == -1) {
    log_(LL_WARN, true, "unable to set the socket send buffer size to %d", val);
    return false;
  }

  // Set the outgoing Time-To-Live value.
  val = (int)op_ttl;
  ret = setsockopt(sock6, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val, sizeof(val));
  if (ret == -1) {
    log_(LL_WARN, true, "unable to set time-to-live to %d", val);
    return false;
  }

  return true;
}

static bool

/// Send a request to all selected targets.
/// @return status code
///
/// @param[in] sock socket
/// @param[in] snum sequence number
static bool
send_all_datagrams(int sock, const uint64_t snum)
{
  uint64_t i;
  payload pl;
  struct timespec rts;
  struct timespec mts;
  struct msghdr msg;
  struct iovec data;
  int ret;
  ssize_t n;

  // Populate payload fields.
  (void)memset(&pl, 0, sizeof(pl));
  pl.pl_mgic = NEMO_PAYLOAD_MAGIC;
  pl.pl_fver = NEMO_PAYLOAD_VERSION;
  pl.pl_type = NEMO_PAYLOAD_TYPE_REQUEST;
  pl.pl_port = (uint16_t)op_port;
  pl.pl_ttl1 = (uint8_t)op_ttl;
  pl.pl_snum = snum;
  pl.pl_slen = op_cnt;
  pl.pl_reqk = op_key;

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

  for (i = 0; i < ntgs; i++) {
    // Obtain the current system time.
    ret = clock_gettime(CLOCK_REALTIME, &rts);
    if (ret == -1) {
      log_(LL_WARN, true, "unable to obtain current system time");
      return false;
    }
    tnanos(&pl.pl_rtm1, rts);
     
    // Obtain the current steady time.
    ret = clock_gettime(CLOCK_MONOTONIC, &mts);
    if (ret == -1) {
      log_(LL_WARN, true, "unable to obtain current system time");
      return false;
    }
    tnanos(&pl.pl_rtm1, mts);

    // Assign target address.
    pl.pl_laddr = tgs[i].tg_laddr;
    pl.pl_haddr = tgs[i].tg_haddr;

    // Send the datagram.
    n = sendmsg(sock, &msg, 0);
  }
}

/// Start emitting the network requests.
/// @return status code
static bool
request_loop(void)
{
  uint64_t i;
  payload pl;
  int reti;
  bool retb;
  struct timespec st_ts;
  fd_set rfd;

  log_(LL_INFO, false, "starting the request loop");

  // Add sockets to the event list.
  if (op_ipv4)
    FD_SET(sock4, &rfd);
  if (op_ipv6)
    FD_SET(sock6, &rfd);

  // Emit a selected number of requests.
  for (i = 0; i < op_cnt; i++) {

    // Send all IPv4 requests.
    retb = send_all_datagrams(sock4, i);
    if (!retb) {
      log_(LL_WARN, false, "unable to send all requests");

      // Quit the loop if network transmissions errors should be fatal.
      if (op_err)
        return false;
    }

    // Send all IPv6 requests.
    retb = send_all_datagrams(sock6, i);
    if (!retb) {
      log_(LL_WARN, false, "unable to send all requests");

      // Quit the loop if network transmissions errors should be fatal.
      if (op_err)
        return false;
    }

    // Save the start time.
    reti = clock_gettime(CLOCK_MONOTONIC, &st_ts);
    if (reti == -1) {
      log_(LL_WARN, true, "unable to get current steady time");
      return false;
    }
    tnanos(&st_ns, st_ts);

    while (true) {
      reti = clock_gettime(CLOCK_MONOTONIC);
      if (reti == -1) {
        log_(LL_WARN, true, "unable to get current steady time");
	return false;
      }

      reti = pselect(5, &rfd, NULL, NULL, rem_ts, NULL);
      if (reti == 
    }
  }

  return true;
}

/// Unicast network requester.
int
main(int argc, char* argv[])
{
  bool retb;

  // Parse command-line arguments.
  retb = parse_arguments(argc, argv);
  if (retb == false) {
    log_(LL_ERROR, false, "unable to parse command-line arguments");
    return EXIT_FAILURE;
  }

  // Parse network targets.
  retb = parse_targets();
  if (retb == false) {
    log_(LL_ERROR, false, "unable to parse network targets");
    return EXIT_FAILURE;
  }

  // Install signal handlers.
  retb = install_signal_handler();
  if (retb == false) {
    log_(LL_ERROR, false, "unable to install signal handlers");
    return EXIT_FAILURE;
  }

  // Create the IPv4 socket.
  retb = create_socket4();
  if (retb == false) {
    log_(LL_ERROR, false, "unable to create the IPv4 socket");
    return EXIT_FAILURE;
  }

  // Create the IPv6 socket.
  retb = create_socket6();
  if (retb == false) {
    log_(LL_ERROR, false, "unable to create the IPv6 socket");
    return EXIT_FAILURE;
  }

  // Start emitting requests.
  retb = request_loop();
  if (retb == false) {
    log_(LL_ERROR, false, "the request loop has ended unexpectedly");
    return EXIT_FAILURE;
  }
    
  return EXIT_SUCCESS;
}
