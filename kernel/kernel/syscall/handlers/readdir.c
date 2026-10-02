/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <kernel/handle/dir.h>
#include <kernel/handle/handle.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <kernel/mem/usr.h>
#include <panuti/errno.h>
#include <panuti/dirent.h>
#include <stdint.h>
#include <string.h>

int32_t syshandler_readdir(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3; (void)a4;
	int fd = (int)a1;
	dirent_entry_t* user_out = (dirent_entry_t*)a2;

	if (!kernel_is_user_range(user_out, sizeof(dirent_entry_t))) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();

	if (fd < 0 || fd >= MAX_HANDLES || t->handles[fd].type == INODE_NONE) {
		return PANUTIERRNO_BADFD;
	}

	if (t->handles[fd].type != INODE_DIR) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	dir_handle_t* dh = t->handles[fd].impl;
	inode_t* dir_inode = t->handles[fd].inode;

	dirent_entry_t entry_out;
	int rc;

	if (dir_inode->mnt) {
		// a backend is not required to implement readdir (fat has none), so
		// check before calling through or we jump into a null pointer
		if (!dir_inode->mnt->fs_ops->readdir) {
			return PANUTIERRNO_UNSUPPORTEDOP;
		}

		rc = dir_inode->mnt->fs_ops->readdir(
			dir_inode->mnt->fs_impl,
			dir_inode,
			&entry_out,
			&dh->cursor
		);
	} else {
		dirent_t* d = dh->next_inode;
		if (!d) {
			rc = 1; // end of directory
		} else {
			size_t n = d->name_len;
			if (n >= sizeof(entry_out.name)) {
				n = sizeof(entry_out.name) - 1;
			}
			
			memcpy(entry_out.name, d->name, n);
			entry_out.name[n] = '\0';
			entry_out.type = d->inode->type;
			entry_out.size = 0;
			dir_handle_advance(dh);
			rc = 0;
		}
	}

	if (rc == 0) {
		*user_out = entry_out;
	}

	return rc;
}