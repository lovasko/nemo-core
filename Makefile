#  Copyright (c) 2018-2019 Daniel Lovasko
#  All Rights Reserved
#
#  Distributed under the terms of the 2-clause BSD License. The full
#  license is in the file LICENSE, distributed as part of this software.

CC = cc
FTM = -D_BSD_SOURCE -D_XOPEN_SOURCE -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CHECKS = -Wall -Wextra -Wconversion -fstrict-aliasing
CFLAGS = -fno-builtin -std=c99 -Werror $(CHECKS) $(FTM) -Isrc/
LDFLAGS = -lrt -ldl

all: bin/ureq bin/ures

# unicast requester executable
bin/ureq: obj/common/convert.o \
          obj/common/log.o     \
          obj/common/now.o     \
          obj/common/parse.o   \
          obj/common/payload.o \
          obj/common/signal.o  \
          obj/common/stats.o   \
          obj/common/ttl.o     \
          obj/ureq/config.o    \
          obj/ureq/event.o     \
          obj/ureq/loop.o      \
          obj/ureq/main.o      \
          obj/ureq/relay.o     \
          obj/ureq/report.o    \
          obj/ureq/round.o     \
          obj/ureq/socket.o    \
          obj/ureq/target.o
	$(CC) -o bin/ureq    \
  obj/common/convert.o \
  obj/common/log.o     \
  obj/common/now.o     \
  obj/common/parse.o   \
  obj/common/payload.o \
  obj/common/signal.o  \
  obj/common/stats.o   \
  obj/common/ttl.o     \
  obj/ureq/config.o    \
  obj/ureq/event.o     \
  obj/ureq/loop.o      \
  obj/ureq/main.o      \
  obj/ureq/relay.o     \
  obj/ureq/report.o    \
  obj/ureq/round.o     \
  obj/ureq/socket.o    \
  obj/ureq/target.o    \
  $(LDFLAGS)

# unicast responder executable
bin/ures: obj/common/convert.o \
          obj/common/log.o     \
          obj/common/now.o     \
          obj/common/parse.o   \
          obj/common/payload.o \
          obj/common/plugin.o  \
          obj/common/signal.o  \
          obj/common/stats.o   \
          obj/common/ttl.o     \
          obj/ures/config.o    \
          obj/ures/event.o     \
          obj/ures/loop.o      \
          obj/ures/main.o      \
          obj/ures/report.o    \
          obj/ures/socket.o
	$(CC) -o bin/ures    \
  obj/common/convert.o \
  obj/common/log.o     \
  obj/common/now.o     \
  obj/common/parse.o   \
  obj/common/payload.o \
  obj/common/plugin.o  \
  obj/common/signal.o  \
  obj/common/stats.o   \
  obj/common/ttl.o     \
  obj/ures/config.o    \
  obj/ures/event.o     \
  obj/ures/loop.o      \
  obj/ures/main.o      \
  obj/ures/report.o    \
  obj/ures/socket.o    \
  $(LDFLAGS)

# unicast requester object files
obj/ureq/config.o: src/ureq/config.c
	$(CC) $(CFLAGS) -c src/ureq/config.c    -o obj/ureq/config.o

obj/ureq/event.o: src/ureq/event.c
	$(CC) $(CFLAGS) -c src/ureq/event.c     -o obj/ureq/event.o

obj/ureq/loop.o: src/ureq/loop.c
	$(CC) $(CFLAGS) -c src/ureq/loop.c      -o obj/ureq/loop.o

obj/ureq/main.o: src/ureq/main.c
	$(CC) $(CFLAGS) -c src/ureq/main.c      -o obj/ureq/main.o

obj/ureq/relay.o: src/ureq/relay.c
	$(CC) $(CFLAGS) -c src/ureq/relay.c     -o obj/ureq/relay.o

obj/ureq/report.o: src/ureq/report.c
	$(CC) $(CFLAGS) -c src/ureq/report.c    -o obj/ureq/report.o

obj/ureq/round.o: src/ureq/round.c
	$(CC) $(CFLAGS) -c src/ureq/round.c     -o obj/ureq/round.o

obj/ureq/socket.o: src/ureq/socket.c
	$(CC) $(CFLAGS) -c src/ureq/socket.c    -o obj/ureq/socket.o

obj/ureq/target.o: src/ureq/target.c
	$(CC) $(CFLAGS) -c src/ureq/target.c    -o obj/ureq/target.o

# unicast responder object files
obj/ures/config.o: src/ures/config.c
	$(CC) $(CFLAGS) -c src/ures/config.c    -o obj/ures/config.o

obj/ures/event.o: src/ures/event.c
	$(CC) $(CFLAGS) -c src/ures/event.c     -o obj/ures/event.o

obj/ures/loop.o: src/ures/loop.c
	$(CC) $(CFLAGS) -c src/ures/loop.c      -o obj/ures/loop.o

obj/ures/main.o: src/ures/main.c
	$(CC) $(CFLAGS) -c src/ures/main.c      -o obj/ures/main.o

obj/ures/report.o: src/ures/report.c
	$(CC) $(CFLAGS) -c src/ures/report.c    -o obj/ures/report.o

obj/ures/socket.o: src/ures/socket.c
	$(CC) $(CFLAGS) -c src/ures/socket.c    -o obj/ures/socket.o

# common object files
obj/common/convert.o: src/common/convert.c
	$(CC) $(CFLAGS) -c src/common/convert.c -o obj/common/convert.o

obj/common/log.o: src/common/log.c
	$(CC) $(CFLAGS) -c src/common/log.c     -o obj/common/log.o

obj/common/now.o: src/common/now.c
	$(CC) $(CFLAGS) -c src/common/now.c     -o obj/common/now.o

obj/common/parse.o: src/common/parse.c
	$(CC) $(CFLAGS) -c src/common/parse.c   -o obj/common/parse.o

obj/common/payload.o: src/common/payload.c
	$(CC) $(CFLAGS) -c src/common/payload.c -o obj/common/payload.o

obj/common/plugin.o: src/common/plugin.c
	$(CC) $(CFLAGS) -c src/common/plugin.c  -o obj/common/plugin.o

obj/common/signal.o: src/common/signal.c
	$(CC) $(CFLAGS) -c src/common/signal.c  -o obj/common/signal.o

obj/common/stats.o: src/common/stats.c
	$(CC) $(CFLAGS) -c src/common/stats.c   -o obj/common/stats.o

obj/common/ttl.o: src/common/ttl.c
	$(CC) $(CFLAGS) -c src/common/ttl.c     -o obj/common/ttl.o

clean:
	rm -f bin/ureq
	rm -f bin/ures
	rm -f obj/common/convert.o
	rm -f obj/common/log.o
	rm -f obj/common/now.o
	rm -f obj/common/parse.o
	rm -f obj/common/payload.o
	rm -f obj/common/plugin.o
	rm -f obj/common/signal.o
	rm -f obj/common/stats.o
	rm -f obj/common/ttl.o
	rm -f obj/ureq/config.o
	rm -f obj/ureq/event.o
	rm -f obj/ureq/loop.o
	rm -f obj/ureq/main.o
	rm -f obj/ureq/relay.o
	rm -f obj/ureq/report.o
	rm -f obj/ureq/round.o
	rm -f obj/ureq/socket.o
	rm -f obj/ureq/target.o
	rm -f obj/ures/config.o
	rm -f obj/ures/event.o
	rm -f obj/ures/loop.o
	rm -f obj/ures/main.o
	rm -f obj/ures/report.o
	rm -f obj/ures/socket.o
