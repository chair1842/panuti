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

OBJS=\
main.o \

BINARY=pur

.PHONY: all clean install install-pur
.SUFFIXES: .o .c .S

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS)

.c.o:
	$(CC) -MD -c $< -o $@ -std=gnu23 $(CFLAGS) $(CPPFLAGS)

.S.o:
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

clean:
	rm -f $(BINARY)
	rm -f $(OBJS)
	rm -f $(OBJS:.o=.d)

install: install-pur

install-pur: $(BINARY)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(BINARY) $(DESTDIR)$(BINDIR)/$(BINARY)

-include $(OBJS:.o=.d)