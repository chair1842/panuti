/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/timer.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "time.h"

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

static int clock_read(void* impl, void* buf, size_t len) {
	(void)impl;

	// boot time plus the ticks since boot, both in centiseconds
	int64_t now = timedev_get_bt() + (int64_t)timer_get_ticks();

	char tmp[32];
	struct clock_sink sink = { tmp, 0 };
	fctprintf(clock_putchar, &sink, "%lld\n", (long long)now);

	int m = (int)len;
	if (m > sink.n) {
		m = sink.n;
	}

	char* p = (char*)buf;
	for (int i = 0; i < m; i++) {
		p[i] = tmp[i];
	}

	return m;
}

static const handle_ops_t clock_ops = {
	.read = clock_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.close = op_not_supported_close,
};

void timedev_clock_register() {
	registry_add("/dvc/time/clock", INODE_DVC, nullptr, &clock_ops);
}
