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
LIBDIR?=$(EXEC_PREFIX)/lib

CFLAGS:=$(CFLAGS) -Wall -Wextra -Wa,--noexecstack

BUILDDIR=build
LIBBUILDDIR=$(BUILDDIR)/lib
LIBINCDIR=lib/include

CPPFLAGS:=$(CPPFLAGS) -I$(LIBINCDIR)

NAME=pcrutils
LIB=$(LIBBUILDDIR)/lib$(NAME).a

LIBSRCS:=$(wildcard lib/*.c)
LIBOBJS=$(LIBSRCS:.c=.o)
LIBDEPS=$(LIBOBJS:.o=.d)

UTILS:=$(patsubst src/%.c,%,$(wildcard src/*.c))
BINARIES=$(UTILS:%=$(BUILDDIR)/%)

.PHONY: all clean install install-pcrutils install-libs install-headers

all: $(LIB) $(BINARIES)

lib/%.o: lib/%.c
	$(CC) -MD -std=gnu23 $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LIB): $(LIBOBJS)
	@mkdir -p $(LIBBUILDDIR)
	$(AR) rcs $@ $(LIBOBJS)

# the lib goes after the util so its symbols resolve, static archives are
# only searched for what is still undefined at that point
$(BUILDDIR)/%: src/%.c $(LIB)
	@mkdir -p $(BUILDDIR)
	$(CC) -std=gnu23 $(CFLAGS) $(CPPFLAGS) -o $@ $< $(LIB) $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)
	rm -f $(LIBOBJS) $(LIBDEPS)

install: install-pcrutils

install-pcrutils: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(BINARIES) $(DESTDIR)$(BINDIR)/

-include $(LIBDEPS)