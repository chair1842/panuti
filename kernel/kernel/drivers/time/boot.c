/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
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
} boot_handle_t;

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

static int boot_open(void* impl, void** out_impl) {
	(void)impl;

	boot_handle_t* h = kmalloc(sizeof(*h), alignof(boot_handle_t));
	if (!h) {
		return PANUTIERRNO_PLAINERR;
	}

	struct boot_sink sink = { h->text, 0 };
	fctprintf(boot_putchar, &sink, "%lld\n", (long long)timedev_get_bt());

	h->len = (size_t)sink.n;
	h->pos = 0;

	*out_impl = h;
	return 0;
}

static int boot_read(void* impl, void* buf, size_t len) {
	boot_handle_t* h = (boot_handle_t*)impl;

	if (h->pos >= h->len) {
		return 0;
	}

	size_t avail = h->len - h->pos;
	size_t n = (avail < len) ? avail : len;

	memcpy(buf, h->text + h->pos, n);
	h->pos += n;

	return (int)n;
}

static int boot_close(void* impl, struct task* self) {
	(void)self;
	kfree(impl);
	return 0;
}

static const handle_ops_t boot_ops = {
	.open = boot_open,
	.read = boot_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.close = boot_close,
};

void timedev_boot_register() {
	registry_add("/dvc/time/boot", INODE_DVC, nullptr, &boot_ops);
}
