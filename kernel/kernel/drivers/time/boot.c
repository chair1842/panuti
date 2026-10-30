/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "time.h"

struct boot_sink {
	char* p;
	int n;
};

static void boot_putchar(char c, void* extra) {
	struct boot_sink* s = (struct boot_sink*)extra;
	if (s->n < 31) {
		s->p[s->n] = c;
	}

	s->n++;
}

static int boot_read(void* impl, void* buf, size_t len) {
	(void)impl;

	char tmp[32];
	struct boot_sink sink = { tmp, 0 };
	fctprintf(boot_putchar, &sink, "%lld\n", (long long)timedev_get_bt());

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

static const handle_ops_t boot_ops = {
	.read = boot_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.close = op_not_supported_close,
};

void timedev_boot_register() {
	registry_add("/dvc/time/boot", INODE_DVC, nullptr, &boot_ops);
}
