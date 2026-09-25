# SPDX-License-Identifier: BSD-3-Clause

DEFAULT_HOST!=../default-host.sh
HOST?=DEFAULT_HOST
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
BINDIR?=$(EXEC_PREFIX)/bin
INCLUDEDIR?=$(PREFIX)/include

CFLAGS:=$(CFLAGS) -Wall -Wextra -Wa,--noexecstack

BUILDDIR=build
UTILS:=$(patsubst src/%.c,%,$(wildcard src/*.c))
BINARIES=$(UTILS:%=$(BUILDDIR)/%)

.PHONY: all clean install install-pcrutils

all: $(BINARIES)

$(BUILDDIR)/%: src/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) -std=gnu23 $(CFLAGS) $(CPPFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)

install: install-pcrutils

install-pcrutils: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(BUILDDIR)/* $(DESTDIR)$(BINDIR)/