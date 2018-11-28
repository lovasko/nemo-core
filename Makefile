#  Copyright (c) 2018 Daniel Lovasko
#  All Rights Reserved
#
#  Distributed under the terms of the 2-clause BSD License. The full
#  license is in the file LICENSE, distributed as part of this software.

CC = cc
FTM = -D_BSD_SOURCE -D_XOPEN_SOURCE -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CHECKS = -Wall -Wextra -fstrict-aliasing
CFLAGS = -fno-builtin -std=c99 -Werror $(CHECKS) $(FTM) -Isrc/ -pthread
LDFLAGS = -lrt -lpthread -ldl

all: bin/ureq bin/ures

# unicast requester executable
bin/ureq: obj/common/convert.o \
          obj/common/daemon.o  \
          obj/common/log.o     \
          obj/common/parse.o   \
          obj/common/ttl.o     \
          obj/ureq/config.o    \
          obj/ureq/counters.o  \
          obj/ureq/loop.o      \
          obj/ureq/main.o      \
          obj/ureq/payload.o   \
          obj/ureq/report.o    \
          obj/ureq/signal.o    \
          obj/ureq/socket.o    \
          obj/ureq/target.o    
	$(CC) obj/common/convert.o   \
	      obj/common/daemon.o    \
	      obj/common/log.o       \
	      obj/common/parse.o     \
	      obj/common/ttl.o       \
	      obj/ureq/config.o      \
	      obj/ureq/counters.o    \
	      obj/ureq/loop.o        \
	      obj/ureq/main.o        \
	      obj/ureq/payload.o     \
	      obj/ureq/report.o      \
	      obj/ureq/socket.o      \
	      obj/ureq/signal.o      \
	      obj/ureq/target.o      \
	      -o bin/ureq $(LDFLAGS)

# unicast responder executable
bin/ures: obj/common/convert.o \
          obj/common/daemon.o  \
          obj/common/log.o     \
          obj/common/parse.o   \
          obj/common/ttl.o     \
          obj/ures/config.o    \
          obj/ures/counters.o  \
          obj/ures/event.o     \
          obj/ures/loop.o      \
          obj/ures/main.o      \
          obj/ures/payload.o   \
          obj/ures/plugins.o   \
          obj/ures/report.o    \
          obj/ures/signal.o    \
          obj/ures/socket.o
	$(CC) obj/common/convert.o   \
	      obj/common/daemon.o    \
	      obj/common/log.o       \
	      obj/common/parse.o     \
	      obj/common/ttl.o       \
	      obj/ures/config.o      \
	      obj/ures/counters.o    \
	      obj/ures/event.o       \
	      obj/ures/loop.o        \
	      obj/ures/main.o        \
	      obj/ures/payload.o     \
	      obj/ures/plugins.o     \
	      obj/ures/report.o      \
	      obj/ures/signal.o      \
	      obj/ures/socket.o      \
	      -o bin/ures $(LDFLAGS)

# unicast requester object files
obj/ureq/config.o: src/ureq/config.c
	$(CC) $(CFLAGS) -c src/ureq/config.c    -o obj/ureq/config.o

obj/ureq/counters.o: src/ureq/counters.c
	$(CC) $(CFLAGS) -c src/ureq/counters.c  -o obj/ureq/counters.o

obj/ureq/loop.o: src/ureq/loop.c
	$(CC) $(CFLAGS) -c src/ureq/loop.c      -o obj/ureq/loop.o

obj/ureq/main.o: src/ureq/main.c
	$(CC) $(CFLAGS) -c src/ureq/main.c      -o obj/ureq/main.o

obj/ureq/payload.o: src/ureq/payload.c
	$(CC) $(CFLAGS) -c src/ureq/payload.c   -o obj/ureq/payload.o

obj/ureq/report.o: src/ureq/report.c
	$(CC) $(CFLAGS) -c src/ureq/report.c    -o obj/ureq/report.o

obj/ureq/signal.o: src/ureq/signal.c
	$(CC) $(CFLAGS) -c src/ureq/signal.c    -o obj/ureq/signal.o

obj/ureq/socket.o: src/ureq/socket.c
	$(CC) $(CFLAGS) -c src/ureq/socket.c    -o obj/ureq/socket.o

obj/ureq/target.o: src/ureq/target.c
	$(CC) $(CFLAGS) -c src/ureq/target.c    -o obj/ureq/target.o

# unicast responder object files
obj/ures/config.o: src/ures/config.c
	$(CC) $(CFLAGS) -c src/ures/config.c    -o obj/ures/config.o

obj/ures/counters.o: src/ures/counters.c
	$(CC) $(CFLAGS) -c src/ures/counters.c  -o obj/ures/counters.o

obj/ures/event.o: src/ures/event.c
	$(CC) $(CFLAGS) -c src/ures/event.c     -o obj/ures/event.o

obj/ures/loop.o: src/ures/loop.c
	$(CC) $(CFLAGS) -c src/ures/loop.c      -o obj/ures/loop.o

obj/ures/main.o: src/ures/main.c
	$(CC) $(CFLAGS) -c src/ures/main.c      -o obj/ures/main.o

obj/ures/payload.o: src/ures/payload.c
	$(CC) $(CFLAGS) -c src/ures/payload.c   -o obj/ures/payload.o

obj/ures/plugins.o: src/ures/plugins.c
	$(CC) $(CFLAGS) -c src/ures/plugins.c   -o obj/ures/plugins.o

obj/ures/report.o: src/ures/report.c
	$(CC) $(CFLAGS) -c src/ures/report.c    -o obj/ures/report.o

obj/ures/signal.o: src/ures/signal.c
	$(CC) $(CFLAGS) -c src/ures/signal.c    -o obj/ures/signal.o

obj/ures/socket.o: src/ures/socket.c
	$(CC) $(CFLAGS) -c src/ures/socket.c    -o obj/ures/socket.o

# common object files
obj/common/convert.o: src/common/convert.c
	$(CC) $(CFLAGS) -c src/common/convert.c -o obj/common/convert.o

obj/common/daemon.o: src/common/daemon.c
	$(CC) $(CFLAGS) -c src/common/daemon.c  -o obj/common/daemon.o

obj/common/log.o: src/common/log.c
	$(CC) $(CFLAGS) -c src/common/log.c     -o obj/common/log.o

obj/common/parse.o: src/common/parse.c
	$(CC) $(CFLAGS) -c src/common/parse.c   -o obj/common/parse.o

obj/common/ttl.o: src/common/ttl.c
	$(CC) $(CFLAGS) -c src/common/ttl.c     -o obj/common/ttl.o

clean:
	rm -f bin/ureq
	rm -f bin/ures
	rm -f obj/common/convert.o
	rm -f obj/common/daemon.o
	rm -f obj/common/log.o
	rm -f obj/common/parse.o
	rm -f obj/common/ttl.o
	rm -f obj/ureq/config.o
	rm -f obj/ureq/counters.o
	rm -f obj/ureq/main.o
	rm -f obj/ureq/payload.o
	rm -f obj/ureq/report.o
	rm -f obj/ureq/signal.o
	rm -f obj/ureq/socket.o
	rm -f obj/ureq/target.o
	rm -f obj/ures/config.o
	rm -f obj/ures/counters.o
	rm -f obj/ures/event.o
	rm -f obj/ures/loop.o
	rm -f obj/ures/main.o
	rm -f obj/ures/payload.o
	rm -f obj/ures/plugins.o
	rm -f obj/ures/report.o
	rm -f obj/ures/signal.o
	rm -f obj/ures/socket.o
