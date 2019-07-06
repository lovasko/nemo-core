// Copyright (c) 2018-2019 Daniel Lovasko.
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/select.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>

#include "common/channel.h"
#include "common/log.h"
#include "common/convert.h"
#include "common/now.h"
#include "common/signal.h"
#include "common/packet.h"
#include "ureq/funcs.h"
#include "ureq/types.h"


/// Create a part of a IPv6 address by converting array of bytes into a
/// single integer.
/// @return portion of IPv6 address
///
/// @param[in] ab address bytes 
static uint64_t
ipv6_part(const uint8_t* ab)
{
  uint64_t res;
  uint8_t i;

  res = 0;
  for (i = 0; i < 8; i++) {
    res |= ((uint64_t)ab[i] << (i * 8));
  }

  return res;
}

/// Retrieve the IP address from a socket address.
///
/// @param[out] la low address bits
/// @param[out] ha high address bits
/// @param[in]  ss IPv4/IPv6 socket address
static void
retrieve_address(uint64_t* la,
                 uint64_t* ha,
                 const struct sockaddr_storage* ss) 
{
  struct sockaddr_in* s4;
  struct sockaddr_in6* s6;

  // Cast the address to the appropriate format based on the address family.
  if (ss->ss_family == AF_INET) {
    s4  = (struct sockaddr_in*)ss;
    *la = (uint64_t)s4->sin_addr.s_addr;
    *ha = (uint64_t)0;
  } else {
    s6  = (struct sockaddr_in6*)ss;
    *la = ipv6_part(&s6->sin6_addr.s6_addr[0]);
    *ha = ipv6_part(&s6->sin6_addr.s6_addr[7]);
  }
}

/// Handle a network event by attempting to receive responses on all available sockets.
/// @return success/failure indication
///
/// @param[in] ch  channel
/// @param[in] rfd read file descriptors
/// @param[in] hn  local host name
/// @param[in] cf  configuration
static bool
handle_event(struct channel* ch,
             const fd_set* rfd,
             const char hn[static NEMO_HOST_NAME_SIZE],
             const struct config* cf)
{
  int reti;
  bool retb;
  struct payload pl;
  struct sockaddr_storage ss;
  uint8_t ttl;
  uint64_t real;
  uint64_t mono;
  uint64_t la;
  uint64_t ha;

  // Ignore the event in case we are in the monologue mode.
  if (cf->cf_mono == true) {
    return true;
  }

  reti = FD_ISSET(ch->ch_sock, rfd);
  if (reti > 0) {
    retb = receive_packet(ch, &ss, &pl, &ttl, cf->cf_err);
    if (retb == false) {
      return false;
    }
  }

  // Retrieve the address of the responder.
  retrieve_address(&la, &ha, &ss);

  // Capture the time of arrival of the response.
  real = real_now();
  mono = mono_now();

  // Create a report entry based on the received payload.
  report_event(&pl, hn, real, mono, ttl, la, ha, cf);

  // TODO notify plugins

  return true;
}

/// Handle the incoming signal.
/// @return exit/continue decision
///
/// @global sint
/// @global sterm
/// @global susr1
///
/// @param[in] ch channel
/// @param[in] cf configuration
static bool
handle_interrupt(const struct channel* ch, const struct config* cf)
{
  log(LL_TRACE, false, "handling interrupt");

  // Exit upon receiving SIGINT.
  if (sint == true) {
    log(LL_WARN, false, "received the %s signal", "SIGINT");
    return false;
  }

  // Exit upon receiving SIGTERM.
  if (sterm == true) {
    log(LL_WARN, false, "received the %s signal", "SIGTERM");
    return false;
  }

  // Print logging information and continue the process upon receiving SIGUSR1.
  if (susr1 == true) {
    log_config(cf);
    log_channel(ch);

    // Reset the signal indicator, so that following signal handling will avoid
    // the false positive.
    susr1 = false;
    return true;
  }

  log(LL_WARN, false, "unknown interrupt occurred");
  return false;
}

/// Await and handle responses on both IPv4 and IPv6 sockets for a selected
/// duration of time.
/// @return success/failure indication
///
/// @global sint
/// @global sterm
/// @global susr1
///
/// @param[in] ch  channel
/// @param[in] dur duration to wait for responses
/// @param[in] hn  local host name
/// @param[in] cf  configuration
bool
wait_for_events(struct channel* ch,
                const uint64_t dur,
                const char hn[static NEMO_HOST_NAME_SIZE],
                const struct config* cf)
{
  int reti;
  uint64_t cur;
  uint64_t goal;
  struct timespec todo;
  fd_set rfd;
  sigset_t mask;
  bool retb;

  // Create the signal mask used for enabling signals during the pselect(2) waiting.
  create_signal_mask(&mask);

  // Set the goal time to be in the future.
  cur  = mono_now();
  goal = cur + dur;

  // Repeat the waiting process until the sufficient time has passed.
  while (cur < goal) {
    log(LL_TRACE, false, "waiting for responses");

    // Compute the time left to wait for the responses.
    fnanos(&todo, goal - cur);

    // Ensure that all relevant events are registered.
    FD_ZERO(&rfd);
    FD_SET(ch->ch_sock, &rfd);

    // Start waiting on events. The number of file descriptors is based on the fact
    // that the process is expected to have three standard streams open, plus socket, plus one.
    reti = pselect(4, &rfd, NULL, NULL, &todo, &mask);
    if (reti == -1) {
      // Check for interrupt (possibly due to a signal).
      if (errno == EINTR) {
        retb = handle_interrupt(ch, cf);
        if (retb == true) {
          continue;
        }

        return false;
      }

      log(LL_WARN, true, "waiting for events failed");
      return false;
    }

    // Handle the network events by receiving and reporting responses.
    if (reti > 0) {
      retb = handle_event(ch, &rfd, hn, cf);
      if (retb == false) {
        return false;
      }
    }

    // Update the current time.
    cur = mono_now();
  }

  return true;
}
