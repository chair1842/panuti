/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/dir.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
#include <string.h>
#include <panuti/dirent.h>

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

int dir_readdir(dir_handle_t* dh, inode_t* dir_inode, dirent_entry_t* out) {
	if (dir_inode->mnt) {
		return dir_inode->mnt->fs_ops->readdir(
			dir_inode->mnt->fs_impl,
			dir_inode->impl,
			out,
			&dh->cursor
		);
	}

	if (!dh->next_inode) {
		return 1; // end of directory
	}

	strncpy(out->name, dh->next_inode->name, sizeof(out->name) - 1);
	out->name[sizeof(out->name) - 1] = '\0';
	out->type = dh->next_inode->inode->type;

	dh->next_inode = dh->next_inode->next;
	return 0;
}