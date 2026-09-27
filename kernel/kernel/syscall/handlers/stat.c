/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <kernel/handle/registry.h>
#include <kernel/handle/fs.h>
#include <kernel/mem/usr.h>
#include <panuti/errno.h>
#include <panuti/dirent.h>
#include <stdint.h>
#include <string.h>

int32_t syshandler_stat(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3; (void)a4;
	const char* path = (const char*)a1;
	dirent_entry_t* user_out = (dirent_entry_t*)a2;

	if (!kernel_is_user_range(user_out, sizeof(dirent_entry_t))) {
		return PANUTIERRNO_INVALIDADDR;
	}

	size_t pathlen = 0;
	if (!kernel_is_user_ptr(path) || (pathlen = kernel_user_strlen(path)) == (size_t)-1) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();

	inode_t* n = registry_resolve(t->cwd, path);
	if (!n) {
		return PANUTIERRNO_NOTFOUND;
	}

	// report the entry's own name rather than the path it was reached by, so
	// stat and readdir describe the same file identically. paths that have no
	// final component to report ("/") keep the path the caller handed us.
	const char* name = path;
	size_t namelen = pathlen;
	inode_t* parent = nullptr;
	const char* last = nullptr;
	size_t lastlen = 0;
	
	if (registry_splitpath(t->cwd, path, &parent, &last, &lastlen) == 0) {
		name = last;
		namelen = lastlen;
	}

	// only a mounted filesystem knows how long its own inodes are; the native
	// registry tree has no lengths recorded anywhere
	size_t size = 0;
	if (n->mnt && n->mnt->fs_ops->size) {
		int64_t reported = n->mnt->fs_ops->size(n->mnt->fs_impl, n);
		if (reported > 0) {
			size = (size_t)reported;
		}
	}

	dirent_entry_t entry;
	memset(&entry, 0, sizeof(entry));

	if (namelen >= sizeof(entry.name)) {
		return PANUTIERRNO_PLAINERR;
	}
	
	memcpy(entry.name, name, namelen);
	entry.name[namelen] = '\0';
	entry.type = n->type;
	entry.size = size;

	*user_out = entry;
	return PANUTIERRNO_PLAINSUCCESS;
}
