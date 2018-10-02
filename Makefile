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
bin/nreq: obj/req/main.o     \
          obj/util/convert.o \
          obj/util/daemon.o  \
          obj/util/log.o     \
          obj/util/parse.o   \
          obj/util/ttl.o
	$(CC) obj/req/main.o       \
	      obj/util/convert.o   \
	      obj/util/daemon.o    \
	      obj/util/log.o       \
	      obj/util/parse.o     \
	      obj/util/ttl.o       \
	      -o bin/nreq $(LDFLAGS)

# responder executable
bin/nres: obj/res/event.o    \
          obj/res/main.o     \
          obj/res/options.o  \
          obj/res/payload.o  \
          obj/res/plugins.o  \
          obj/res/report.o   \
          obj/res/socket.o   \
          obj/util/convert.o \
          obj/util/daemon.o  \
          obj/util/log.o     \
          obj/util/parse.o   \
          obj/util/ttl.o
	$(CC) obj/res/event.o      \
	      obj/res/main.o       \
	      obj/res/options.o    \
	      obj/res/payload.o    \
	      obj/res/plugins.o    \
	      obj/res/report.o     \
	      obj/res/socket.o     \
	      obj/util/convert.o   \
	      obj/util/daemon.o    \
	      obj/util/log.o       \
	      obj/util/parse.o     \
	      obj/util/ttl.o       \
	      -o bin/nres $(LDFLAGS)

# requester object files
obj/req/main.o: src/req/main.c
	$(CC) $(CFLAGS) -c src/req/main.c -o obj/req/main.o

# responder object files
obj/res/event.o: src/res/event.c
	$(CC) $(CFLAGS) -c src/res/event.c -o obj/res/event.o

obj/res/main.o: src/res/main.c
	$(CC) $(CFLAGS) -c src/res/main.c -o obj/res/main.o

obj/res/options.o: src/res/options.c
	$(CC) $(CFLAGS) -c src/res/options.c -o obj/res/options.o

obj/res/payload.o: src/res/payload.c
	$(CC) $(CFLAGS) -c src/res/payload.c -o obj/res/payload.o

obj/res/plugins.o: src/res/plugins.c
	$(CC) $(CFLAGS) -c src/res/plugins.c -o obj/res/plugins.o

obj/res/report.o: src/res/report.c
	$(CC) $(CFLAGS) -c src/res/report.c -o obj/res/report.o

obj/res/socket.o: src/res/socket.c
	$(CC) $(CFLAGS) -c src/res/socket.c -o obj/res/socket.o

# utility object files
obj/util/convert.o: src/util/convert.c
	$(CC) $(CFLAGS) -c src/util/convert.c -o obj/util/convert.o

obj/util/daemon.o: src/util/daemon.c
	$(CC) $(CFLAGS) -c src/util/daemon.c -o obj/util/daemon.o

obj/util/log.o: src/util/log.c
	$(CC) $(CFLAGS) -c src/util/log.c -o obj/util/log.o

obj/util/parse.o: src/util/parse.c
	$(CC) $(CFLAGS) -c src/util/parse.c -o obj/util/parse.o

obj/util/ttl.o: src/util/ttl.c
	$(CC) $(CFLAGS) -c src/util/ttl.c -o obj/util/ttl.o

clean:
	rm -f bin/nreq
	rm -f bin/nres
	rm -f obj/req/main.o
	rm -f obj/res/event.o
	rm -f obj/res/main.o
	rm -f obj/res/options.o
	rm -f obj/res/payload.o
	rm -f obj/res/plugins.o
	rm -f obj/res/report.o
	rm -f obj/res/socket.o
	rm -f obj/util/convert.o
	rm -f obj/util/daemon.o
	rm -f obj/util/log.o
	rm -f obj/util/parse.o
	rm -f obj/util/ttl.o
