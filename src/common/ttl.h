// Copyright (c) 2018 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_UTIL_TTL_H
#define NEMO_UTIL_TTL_H

#include <sys/socket.h>

#include <stdbool.h>


bool retrieve_ttl(int* ttl, struct msghdr* msg);

#endif
