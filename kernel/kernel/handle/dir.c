/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/dir.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>

void dir_handle_start(dir_handle_t* dh, dirent_t* entry) {
	dh->cursor = 0;
	dh->next_inode = entry;
	if (entry) {
		dirent_ref(entry);
	}
}

void dir_handle_advance(dir_handle_t* dh) {
	dirent_t* current = dh->next_inode;
	if (!current) {
		return;
	}

	dirent_t* next = current->next;
	if (next) {
		dirent_ref(next);
	}

	dh->next_inode = next;
	dirent_unref(current);
}

static int dir_read(void* impl, void* buf, size_t len) {
	(void)impl; (void)buf; (void)len;
	return -1;
}

static int dir_write(void* impl, const void* buf, size_t len) {
	(void)impl; (void)buf; (void)len;
	return -1;
}

static int dir_activate(void* impl) {
	(void)impl;
	return -1;
}

static int dir_ready(void* impl) {
	(void)impl;
	return -1;
}

static int dir_close(void* impl, struct task* self) {
	(void)self;

	dir_handle_t* dh = impl;
	if (dh) {
		dirent_unref(dh->next_inode);
		dh->next_inode = nullptr;
	}
	
	kfree(impl);
	return 0;
}

const handle_ops_t dir_handle_ops = {
	.read = dir_read,
	.write = dir_write,
	.activate = dir_activate,
	.ready = dir_ready,
	.close = dir_close,
};