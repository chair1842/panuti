/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
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
} uptime_handle_t;

struct uptime_sink {
	char* p;
	int n;
};

static void uptime_putchar(char c, void* extra) {
	struct uptime_sink* s = (struct uptime_sink*)extra;
	if (s->n < 31) {
		s->p[s->n] = c;
	}

	s->n++;
}

static int uptime_open(void* impl, void** out_impl) {
	(void)impl;

	uptime_handle_t* h = kmalloc(sizeof(*h), alignof(uptime_handle_t));
	if (!h) {
		return PANUTIERRNO_PLAINERR;
	}

	// the PIT runs at 100 Hz, so one tick is 10 ms (a centisecond)
	uint64_t centis = timer_get_ticks();

	struct uptime_sink sink = { h->text, 0 };
	fctprintf(uptime_putchar, &sink, "%llu\n", (unsigned long long)centis);

	h->len = (size_t)sink.n;
	h->pos = 0;

	*out_impl = h;
	return 0;
}

static int uptime_read(void* impl, void* buf, size_t len) {
	uptime_handle_t* h = (uptime_handle_t*)impl;

	if (h->pos >= h->len) {
		return 0;
	}

	size_t avail = h->len - h->pos;
	size_t n = (avail < len) ? avail : len;

	memcpy(buf, h->text + h->pos, n);
	h->pos += n;

	return (int)n;
}

static int uptime_close(void* impl, struct task* self) {
	(void)self;
	kfree(impl);
	return 0;
}

static const handle_ops_t uptime_ops = {
	.open = uptime_open,
	.read = uptime_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.close = uptime_close,
};

void timedev_uptime_register() {
	registry_add("/dvc/time/uptime", INODE_DVC, nullptr, &uptime_ops);
}
