# nemo-core
Lightweight network monitoring for POSIX systems.

[![Build Status](https://travis-ci.org/lovasko/nemo-core.svg?branch=master)](https://travis-ci.org/lovasko/nemo-core)

[![Coverity Scan](https://scan.coverity.com/projects/16148/badge.svg)](https://scan.coverity.com/projects/lovasko-nemo-core)

## Introduction
The `nemo-core` project consists of two programs - the _requester_ and the
_responder_ - named `nreq` and `nres` respectively. The requester program
periodically issues UDP datagram requests to a selected set of network targets,
whereas the responder program awaits the requests and issues the appropriate
responses. The datagram payload contains diagnostic information that can be
used to test and monitor unicast network configuration.

## Action sets
In order to make the programs as reusable as possible, the decision was taken
to perform no particular action upon any events - but rather provide a plugin
mechanism via standard `dlopen(3)` mechanism of shared objects. Each program
provides a different set of actions, which are executed upon the triggering.
Example action sets are:

 * `nemo-csv`
 * `nemo-sqlite`

## Example usage
First step is to start the responder process
 * specifying its unique key to be `123`
 * adding action set `nemo-res-csv.so`

```sh
dlo@linux$ nres \
 -k 123         \
 -a nemo-res-csv.so`
```

Second step is to start the requestor process:
 * specifying its unique key to be `456`
 * adding action set `nemo-req-csv.so`
 * setting one target: DNS name `linux`

```sh
dlo@freebsd$ nreq    \
  -k 456             \
  -a nemo-req-csv.so \
  linux
```

## Build
To build `nemo-core` on Linux:
```
dlo@linux$ make CC=gcc
dlo@linux$ make install
```

To build `nemo-core` on FreeBSD:
```
dlo@freebsd$ make CC=clang FTM= 
dlo@freebsd$ make install
```

### Supported platforms
The project aims at supporting 32-bit and 64-bit architectures, Linux, FreeBSD,
NetBSD, and OpenBSD operating systems and all major C compilers, e.g. `gcc` and
`clang`. If any combination of the above does not work, please feel free to
notify the project maintainers and/or submit a patch.

## Documentation
The `nreq` and `nres` programs are documented via standard UNIX manual pages,
located in the `man/` directory. Both manual pages belong the section 8 of the
manual.  The `make install` command copies the manual page files to the
standard system location. All reasonable distributions of the software suite
should have the manual pages bundled in.

## Code standards
The codebase is written in pure C99 while being fully compliant with the
POSIX.1-2018 interfaces. All further code contributions must adhere to these
standards.

## Resource usage
One of the main implementation goals was to create programs that use the
minimal possible resources. This is reflected in both file descriptor, and
memory usage. Both programs use a constant amount of memory and only two open
sockets at any time. No dynamic allocation is performed by either program
throughout the whole lifetime of both processes.

## Limitations
Certain trade-offs were part of the design and implementation of the programs.
The main limitation throughout the software suite is the upper bound of number
of targets supported in each requester process: _512_. The second main
limitation is the number of actions sets attachable to a single process: _32_.

## License
The `nemo-core` project is licensed under the terms of the [2-cause BSD
license](LICENSE).

## Author
Daniel Lovasko <daniel.lovasko@gmail.com>
