/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <stddef.h>
#include <string.h>
#include "kdev.h"

static int zero_read(void* impl, void* buf, size_t len) {
	(void)impl;
	memset(buf, 0, len);
	return (int)len;
}

static int zero_write(void* impl, const void* buf, size_t len) {
	(void)impl; (void)buf;
	return (int)len;
}

static const handle_ops_t zero_ops = {
	.read = zero_read,
	.write = zero_write,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.close = op_not_supported_close,
};

void kdev_zero_register() {
	registry_add("/dvc/zero", INODE_FILE, nullptr, &zero_ops);
}