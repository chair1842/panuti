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
INCLUDEDIR?=$(PREFIX)/include

CFLAGS:=$(CFLAGS) -Wall -Wextra -Wa,--noexecstack

NAME=pint
SRCS:=$(wildcard src/*.c)
OBJS:=$(SRCS:.c=.o)
DEPS:=$(OBJS:.o=.d)

BINARY=$(NAME)

.PHONY: all clean install install-pint

all: $(BINARY)

src/%.o: src/%.c
	$(CC) -MD -std=gnu23 $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BINARY): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS)

clean:
	rm -f $(BINARY) $(OBJS) $(DEPS)

install: install-pint

install-pint: $(BINARY)
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp $(BINARY) $(DESTDIR)$(BOOTDIR)/$(NAME)

-include $(DEPS)
