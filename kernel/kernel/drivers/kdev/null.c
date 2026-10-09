/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <stddef.h>
#include "kdev.h"

static int null_read(void* impl, void* buf, size_t len) {
	(void)impl; (void)buf; (void)len;
	return 0;
}

static int null_write(void* impl, const void* buf, size_t len) {
	(void)impl; (void)buf;
	return (int)len;
}

static const handle_ops_t null_ops = {
	.read = null_read,
	.write = null_write,
	.activate = op_not_supported_act,
	.seek = op_seek_ignore,
	.ready = op_not_supported_rdy,
	.close = op_not_supported_close,
};

void kdev_null_register() {
	registry_add("/dvc/null", INODE_FILE, nullptr, &null_ops);
}