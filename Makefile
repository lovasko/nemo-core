#  Copyright (c) 2018 Daniel Lovasko.
#  All Rights Reserved
#
#  Distributed under the terms of the 2-clause BSD License. The full
#  license is in the file LICENSE, distributed as part of this software.

CC = gcc
FTM = -D_BSD_SOURCE -D_XOPEN_SOURCE -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CFLAGS = -fno-builtin -std=c99 -Wall -Wextra -Werror $(FTM) -Isrc/ -pthread
LDFLAGS = -lrt -lpthread -ldl

all: bin/nreq bin/nres

# requester executable
bin/nreq: obj/req/main.o       \
          obj/common/convert.o \
          obj/common/daemon.o  \
          obj/common/log.o     \
          obj/common/parse.o   \
          obj/common/ttl.o
	$(CC) obj/req/main.o         \
	      obj/common/convert.o   \
	      obj/common/daemon.o    \
	      obj/common/log.o       \
	      obj/common/parse.o     \
	      obj/common/ttl.o       \
	      -o bin/nreq $(LDFLAGS)

# responder executable
bin/nres: obj/common/convert.o \
          obj/common/daemon.o  \
          obj/common/log.o     \
          obj/common/parse.o   \
          obj/common/ttl.o     \
          obj/res/config.o     \
          obj/res/counters.o   \
          obj/res/event.o      \
          obj/res/loop.o       \
          obj/res/main.o       \
          obj/res/payload.o    \
          obj/res/plugins.o    \
          obj/res/report.o     \
          obj/res/signal.o     \
          obj/res/socket.o
	$(CC) obj/common/convert.o   \
	      obj/common/daemon.o    \
	      obj/common/log.o       \
	      obj/common/parse.o     \
	      obj/common/ttl.o       \
	      obj/res/config.o       \
	      obj/res/counters.o     \
	      obj/res/event.o        \
	      obj/res/loop.o         \
	      obj/res/main.o         \
	      obj/res/payload.o      \
	      obj/res/plugins.o      \
	      obj/res/report.o       \
	      obj/res/signal.o       \
	      obj/res/socket.o       \
	      -o bin/nres $(LDFLAGS)

# requester object files
obj/req/main.o: src/req/main.c
	$(CC) $(CFLAGS) -c src/req/main.c       -o obj/req/main.o

# responder object files
obj/res/config.o: src/res/config.c
	$(CC) $(CFLAGS) -c src/res/config.c     -o obj/res/config.o

obj/res/counters.o: src/res/counters.c
	$(CC) $(CFLAGS) -c src/res/counters.c   -o obj/res/counters.o

obj/res/event.o: src/res/event.c
	$(CC) $(CFLAGS) -c src/res/event.c      -o obj/res/event.o

obj/res/loop.o: src/res/loop.c
	$(CC) $(CFLAGS) -c src/res/loop.c       -o obj/res/loop.o

obj/res/main.o: src/res/main.c
	$(CC) $(CFLAGS) -c src/res/main.c       -o obj/res/main.o

obj/res/payload.o: src/res/payload.c
	$(CC) $(CFLAGS) -c src/res/payload.c    -o obj/res/payload.o

obj/res/plugins.o: src/res/plugins.c
	$(CC) $(CFLAGS) -c src/res/plugins.c    -o obj/res/plugins.o

obj/res/report.o: src/res/report.c
	$(CC) $(CFLAGS) -c src/res/report.c     -o obj/res/report.o

obj/res/signal.o: src/res/signal.c
	$(CC) $(CFLAGS) -c src/res/signal.c     -o obj/res/signal.o

obj/res/socket.o: src/res/socket.c
	$(CC) $(CFLAGS) -c src/res/socket.c     -o obj/res/socket.o

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
	rm -f bin/nreq
	rm -f bin/nres
	rm -f obj/common/convert.o
	rm -f obj/common/daemon.o
	rm -f obj/common/log.o
	rm -f obj/common/parse.o
	rm -f obj/common/ttl.o
	rm -f obj/req/main.o
	rm -f obj/res/config.o
	rm -f obj/res/counters.o
	rm -f obj/res/event.o
	rm -f obj/res/loop.o
	rm -f obj/res/main.o
	rm -f obj/res/payload.o
	rm -f obj/res/plugins.o
	rm -f obj/res/report.o
	rm -f obj/res/signal.o
	rm -f obj/res/socket.o
