/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/registry.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>

int32_t syshandler_chdir(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	size_t path_len = kernel_user_strlen(path);
	if (path_len == (size_t)-1 || !kernel_usrptr_verify(path, path_len + 1)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();

	inode_t* n = registry_resolve(t->cwd, path);
	if (!n) {
		return PANUTIERRNO_NOTFOUND;
	}
	if (n->type != INODE_DIR) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	task_set_cwd(t, n);
	return 0;
}
