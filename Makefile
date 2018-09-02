#  Copyright (c) 2018 Daniel Lovasko.
#  All Rights Reserved
#
#  Distributed under the terms of the 2-clause BSD License. The full
#  license is in the file LICENSE, distributed as part of this software.

CC = gcc
FTM = -D_BSD_SOURCE -D_XOPEN_SOURCE -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CFLAGS = -fno-builtin -std=c99 -Wall -Wextra -Werror $(FTM) -Isrc/ -pthread
LDFLAGS = -lrt -lpthread

all: bin/nreq bin/nres

# executables
bin/nreq: obj/req/main.o obj/util/convert.o obj/util/daemon.o obj/util/log.o obj/util/parse.o
	$(CC) obj/req/main.o obj/util/convert.o obj/util/daemon.o obj/util/log.o obj/util/parse.o -o bin/nreq $(LDFLAGS)

bin/nres: obj/res/main.o obj/util/convert.o obj/util/daemon.o obj/util/log.o obj/util/parse.o
	$(CC) obj/res/main.o obj/util/convert.o obj/util/daemon.o obj/util/log.o obj/util/parse.o -o bin/nres $(LDFLAGS)

# program object files
obj/req/main.o: src/req/main.c
	$(CC) $(CFLAGS) -c src/req/main.c -o obj/req/main.o

obj/res/main.o: src/res/main.c
	$(CC) $(CFLAGS) -c src/res/main.c -o obj/res/main.o

# utility object files
obj/util/convert.o: src/util/convert.c
	$(CC) $(CFLAGS) -c src/util/convert.c -o obj/util/convert.o

obj/util/daemon.o: src/util/daemon.c
	$(CC) $(CFLAGS) -c src/util/daemon.c -o obj/util/daemon.o

obj/util/log.o: src/util/log.c
	$(CC) $(CFLAGS) -c src/util/log.c -o obj/util/log.o

obj/util/parse.o: src/util/parse.c
	$(CC) $(CFLAGS) -c src/util/parse.c -o obj/util/parse.o

clean:
	rm -f bin/nreq
	rm -f bin/nres
	rm -f obj/req/main.o
	rm -f obj/res/main.o
	rm -f obj/util/convert.o
	rm -f obj/util/daemon.o
	rm -f obj/util/log.o
	rm -f obj/util/parse.o
