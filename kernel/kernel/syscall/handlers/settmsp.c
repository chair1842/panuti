/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/fs.h>
#include <kernel/handle/registry.h>
#include <kernel/mem/usr.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <kernel/syscall/handlers.h>
#include <panuti/errno.h>
#include <panuti/inode_attr.h>
#include <stdint.h>
#include <string.h>

int32_t syshandler_settmsp(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a4;
	const char* path = (const char*)a1;
	const inode_timestamp_t* user_times = (const inode_timestamp_t*)a2;
	uint32_t mask = a3;

	size_t path_len = kernel_user_strlen(path);
	if (path_len == (size_t)-1 || !kernel_usrptr_verify(path, path_len + 1)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (!kernel_usrptr_verify(user_times, sizeof(inode_timestamp_t))) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (mask & ~INODE_ATTR_TIMES) {
		return PANUTIERRNO_INVALIDARG;
	}

	task_t* t = sched_current();
	inode_t* n = registry_resolve(t->cwd, path);
	if (!n) {
		return PANUTIERRNO_NOTFOUND;
	}

	if (!n->mnt || !n->mnt->fs_ops->setattr) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	inode_timestamp_t times;
	memcpy(&times, user_times, sizeof(times));

	inode_attr_t attr = {
		.valid = mask,
		.atime = times.atime,
		.mtime = times.mtime,
		.ctime = times.ctime,
		.btime = times.btime,
	};

	return n->mnt->fs_ops->setattr(n->mnt->fs_impl, n, &attr, mask);
}
