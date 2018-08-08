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
#define DEF_LOG_LEVEL           LL_DEBUG    // Log errors and warnings by default.
#define DEF_LOG_COLOR           true       // Colors in the notification output.
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
static bool susr1;                        ///< SIGUSR1 flag.
static uint64_t ntgs;                     ///< Number of targets.
static target tgs[NEMO_TARGET_COUNT_MAX]; ///< Target database.
static pthread_t sender4;                 ///< IPv4 sender thread.
static pthread_t recver4;                 ///< IPv4 receiver thread.
static pthread_t sender6;                 ///< IPv6 sender thread.
static pthread_t recver6;                 ///< IPv6 receiver thread.


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
    "  -r RBS  Receive memory buffer size.\n"
    "  -s SBS  Send memory buffer size.\n"
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
  while ((opt = getopt(argc, argv, "46c:ehi:k:np:r:s:t:v")) != -1) {
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

      // Time interval between rounds of sent datagrams.
      case 'i':
        if (parse_scalar(&op_int, optarg, "ns", parse_time_unit) == false)
          return false;
        break;

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
        if (parse_uint64(&op_ttl, optarg, 1, 255) == false)
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

	*pidx = optind;

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
	char str[64];

	memset(str, '\0', sizeof(str));
  
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

			log_(LL_INFO, false, "parsed %s target: %s", "IPv4", argv[i]);
      ntgs++;
      continue;
    }
    
    // Try parsing the address as IPv6.
    reti = inet_pton(AF_INET6, argv[i], &addr6);
    if (reti == 1) {
      tgs[tidx].tg_type  = NEMO_TARGET_TYPE_IPV6;
      tgs[tidx].tg_laddr = make_address_part(&addr6.s6_addr[0]);
      tgs[tidx].tg_haddr = make_address_part(&addr6.s6_addr[7]);

			log_(LL_INFO, false, "parsed %s target: %s", "IPv6", argv[i]);
      ntgs++;
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

  if (sig == SIGUSR1)
		susr1 = true;
}

/// Install signal handlers.
/// @return status code
static bool
install_signal_handler(void)
{
  struct sigaction sa;
  int reti;
  sigset_t sset;

  // Reset the signal indicator.
  sint = false;
  susr1 = false;

  // Create a set of blocked signals.
  (void)sigfillset(&sset);
  (void)sigdelset(&sset, SIGINT);
  (void)sigdelset(&sset, SIGUSR1);
  (void)sigprocmask(SIG_SETMASK, &sset, NULL);

  // Initialise the handler settings.
  (void)memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;

  // Install signal handler for SIGINT.
	log_(LL_INFO, false, "installing signal handler for %s", "SIGINT");
  reti = sigaction(SIGINT, &sa, NULL);
  if (reti == -1) {
    log_(LL_WARN, true, "unable to add signal handler for %s", "SIGINT");
    return false;
  }

  // Install signal handler for SIGUSR1.
	log_(LL_INFO, false, "installing signal handler for %s", "SIGUSR1");
  reti = sigaction(SIGUSR1, &sa, NULL);
  if (reti == -1) {
    log_(LL_WARN, true, "unable to add signal handler for %s", "SIGUSR1");
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
fill_payload(payload *pl,
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
static bool
send_all_datagrams(int sock, const uint64_t snum)
{
  uint64_t i;
  payload pl;
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
		// before the encode_.
    set_target_address(&addr, tgs[i]);

    // Prepare payload.
    (void)clock_gettime(CLOCK_REALTIME,  &rts);
    (void)clock_gettime(CLOCK_MONOTONIC, &mts);
    fill_payload(&pl, &tgs[i], snum, rts, mts);
    encode_payload(&pl);

    // Send the datagram.
    n = sendmsg(sock, &msg, MSG_DONTWAIT);
    if (n == -1) {
      log_(LL_WARN, true, "unable to send datagram");

      if (op_err == false)
        return false;
    }

    // Verify the number of sent bytes.
    if ((size_t)n != sizeof(pl)) {
      log_(LL_WARN, false, "unable to send all data");

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
  int sock;
  uint64_t i;
  int reti;
	bool retb;
  struct timespec dur;

  // Resolve the argument to a socket.
  sock = *(int*)arg;
	log_(LL_INFO, false, "starting sender thread for %s",
	  sock == sock4 ? "IPv4" : "IPv6");

  // Convert the interval duration.
  fnanos(&dur, op_int);

  // Start the sending loop.
  for (i = 0; i < op_cnt; i++) {

    log_(LL_DEBUG, false, "sending datagram round %" PRIu64 "/%" PRIu64, 
      i + 1, op_cnt);

    // Send all datagrams.
    retb = send_all_datagrams(sock, i);
    if (retb == false) {
      log_(LL_WARN, false, "unable to send all payloads to IPv4 socket");

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
            kill_other_threads();
            break;
          }

          if (susr1 == true) {
            log_(LL_INFO, false, "exitting this thread");
            break;
          }
        }

        log_(LL_WARN, true, "unable to sleep");
      }
    }
  }

  return NULL;
}

/// Thread that receives responses on a specified socket.
/// @return NULL
///
/// @param[in] arg socket file descriptor
static void*
recv_worker(void* arg)
{
  int sock;
  ssize_t retss;
	payload pl;
  struct msghdr msg;
  struct iovec iov;
	struct sockaddr_storage addr;

  // Resolve the argument to a socket.
  sock = *(int*)arg;
	log_(LL_INFO, false, "starting receiver thread for %s",
	  sock == sock4 ? "IPv4" : "IPv6");

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
    retss = recvmsg(sock, &msg, MSG_TRUNC);

    if (retss == -1) {
      // Check if we were interrupted by a signal delivery.
      if (errno == EINTR) {
        
        // If the signal was SIGINT, send SIGUSR1 to all other worker threads
        // to interrupt their system calls.
        if (sint == true) {
          kill_other_threads();
          break;
        }

        // Perform orderly exit after receiving the SIGUSR1 signal.
        if (susr1 == true)
          break;
      }

      log_(LL_WARN, true, "unable to receive datagrams");
    }

    log_(LL_DEBUG, false, "received message, responder key = %" PRIu64 "!", pl.pl_resk);
  }

  return NULL;
}

/// Start emitting the network requests.
/// @return status code
static bool
request_loop(void)
{
  int reti;

	log_(LL_INFO, false, "starting the request loop");

  // Start the IPv4 sending/receiving.
  if (op_ipv4 == true) {
    // Create the IPv4 receiver thread.
    reti = pthread_create(&recver4, NULL, recv_worker, &sock4);
    if (reti != 0) {
    }

    // Create the IPv4 sender thread.
    reti = pthread_create(&sender4, NULL, send_worker, &sock4);
    if (reti == -1) {
    }
  }

  // Start the IPv6 sending/receiving.
  if (op_ipv6 == true) {
    // Create the IPv6 receiver thread.
    reti = pthread_create(&recver6, NULL, recv_worker, &sock6);
    if (reti == -1) {
    }

    // Create the IPv6 sender thread.
    reti = pthread_create(&sender6, NULL, send_worker, &sock6);
    if (reti == -1) {
    }
  }

  // Wait for the delivery of the SIGINT signal. Ignore the delivery of any
  // other signal. Since the delivery of a signal can be dispatched to any
  // thread, we need to terminate all worker threads. Once we send the USR1
  // signal to all of them to interrupt the system calls they are  in, we wait
  // for their orderly finish by joining them to the main thread.

  while (true) {
    (void)pause();
    
    if (sint == true) {
      kill_other_threads();
      (void)pthread_join(sender4, NULL);
      (void)pthread_join(recver4, NULL);
      (void)pthread_join(sender6, NULL);
      (void)pthread_join(recver6, NULL);
      break;
    }
  }

  return true;
}


/// Unicast network requester.
int
main(int argc, char* argv[])
{
  bool retb;
	int pidx;

  // Parse command-line options.
  retb = parse_options(&pidx, argc, argv);
  if (retb == false) {
    log_(LL_ERROR, false, "unable to parse command-line options");
    return EXIT_FAILURE;
  }

  // Parse network targets.
  retb = parse_targets(pidx, argc, argv);
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
