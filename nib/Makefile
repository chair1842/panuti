# SPDX-License-Identifier: GPL-2.0-or-later

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
BOOTDIR?=$(EXEC_PREFIX)/boot
BINDIR?=$(EXEC_PREFIX)/bin
INCLUDEDIR?=$(PREFIX)/include

CFLAGS:=$(CFLAGS) -Wall -Wextra -Wa,--noexecstack

NAME=nib
SRCS:=$(wildcard src/*.c)
OBJS:=$(SRCS:.c=.o)
DEPS:=$(OBJS:.o=.d)

BINARY=$(NAME)

.PHONY: all clean install install-nib

all: $(BINARY)

src/%.o: src/%.c
	$(CC) -MD -std=gnu23 $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BINARY): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS)

clean:
	rm -f $(BINARY) $(OBJS) $(DEPS)

install: install-nib

install-nib: $(BINARY)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(BINARY) $(DESTDIR)$(BINDIR)/$(NAME)

-include $(DEPS)
