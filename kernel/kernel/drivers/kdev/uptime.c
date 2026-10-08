/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/timer.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "kdev.h"

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

static int uptime_read(void* impl, void* buf, size_t len) {
	(void)impl;

	// the PIT runs at 100 Hz, so one tick is 10 ms (a centisecond)
	uint64_t centis = timer_get_ticks();

	char tmp[32];
	struct uptime_sink sink = { tmp, 0 };
	fctprintf(uptime_putchar, &sink, "%llu\n", centis);

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

static const handle_ops_t uptime_ops = {
	.read = uptime_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.close = op_not_supported_close,
};

void kdev_uptime_register() {
	registry_add("/dvc/uptime", INODE_FILE, nullptr, &uptime_ops);
}