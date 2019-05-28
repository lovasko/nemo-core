// Copyright (c) 2018-2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#ifndef NEMO_COMMON_SIGNAL_H
#define NEMO_COMMON_SIGNAL_H

#include <stdbool.h>
#include <signal.h>


extern volatile bool sint;
extern volatile bool sterm;
extern volatile bool susr1;
extern volatile bool shup;
extern volatile bool schld;

bool install_signal_handlers(void);
void create_signal_mask(sigset_t* mask);

#endif
