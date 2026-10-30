/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
#include <kernel/rtc.h>
#include <kernel/timer.h>
#include <panuti/errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "time.h"

typedef struct {
	char text[32];
	size_t len;
	size_t pos;
} clock_handle_t;

struct clock_sink {
	char* p;
	int n;
};

static void clock_putchar(char c, void* extra) {
	struct clock_sink* s = (struct clock_sink*)extra;
	if (s->n < 31) {
		s->p[s->n] = c;
	}

	s->n++;
}

static int clock_open(void* impl, void** out_impl) {
	(void)impl;

	clock_handle_t* h = kmalloc(sizeof(*h), alignof(clock_handle_t));
	if (!h) {
		return PANUTIERRNO_PLAINERR;
	}

	// boot time plus the ticks since boot, both in centiseconds
	int64_t now = timedev_get_bt() + (int64_t)timer_get_ticks();

	struct clock_sink sink = { h->text, 0 };
	fctprintf(clock_putchar, &sink, "%lld\n", (long long)now);

	h->len = (size_t)sink.n;
	h->pos = 0;

	*out_impl = h;
	return 0;
}

static int clock_read(void* impl, void* buf, size_t len) {
	clock_handle_t* h = (clock_handle_t*)impl;

	if (h->pos >= h->len) {
		return 0;
	}

	size_t avail = h->len - h->pos;
	size_t n = (avail < len) ? avail : len;

	memcpy(buf, h->text + h->pos, n);
	h->pos += n;

	return (int)n;
}

static int clock_write(void* impl, const void* buf, size_t len) {
	(void)impl;

	const char* in = (const char*)buf;
	size_t i = 0;

	while (i < len && (in[i] == ' ' || in[i] == '\t')) {
		i++;
	}

	if (i >= len || in[i] < '0' || in[i] > '9') {
		return PANUTIERRNO_INVALIDARG;
	}

	int64_t value = 0;

	while (i < len && in[i] >= '0' && in[i] <= '9') {
		int digit = in[i] - '0';

		if (value > (INT64_MAX - digit) / 10) {
			return PANUTIERRNO_INVALIDARG;
		}

		value = value * 10 + digit;
		i++;
	}

	if (rtc_set_timestamp(value) != 0) {
		return PANUTIERRNO_INVALIDARG;
	}

	timedev_set_bt(value - (int64_t)timer_get_ticks());

	return (int)len;
}

static int clock_close(void* impl, struct task* self) {
	(void)self;
	kfree(impl);
	return 0;
}

static const handle_ops_t clock_ops = {
	.open = clock_open,
	.read = clock_read,
	.write = clock_write,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.close = clock_close,
};

void timedev_clock_register() {
	registry_add("/dvc/time/clock", INODE_DVC, nullptr, &clock_ops);
}
