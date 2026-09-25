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

	dirent_entry_t entry;
	int rc;

	if (dir_inode->mnt) {
		rc = dir_inode->mnt->fs_ops->readdir(dir_inode->mnt->fs_impl, dir_inode, &entry, &dh->cursor);
	} else {
		if (!dh->next_inode) {
			rc = 1; // end of directory
		} else {
			strncpy(entry.name, dh->next_inode->name, sizeof(entry.name) - 1);
			entry.name[sizeof(entry.name) - 1] = '\0';
			entry.type = dh->next_inode->inode->type;
			dh->next_inode = dh->next_inode->next;
			rc = 0;
		}
	}

	if (rc == 0) {
		*user_out = entry;
	}

	return rc;
}