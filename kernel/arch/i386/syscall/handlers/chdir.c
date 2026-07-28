#include <kernel/handle/registry.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include "handlers.h"

int32_t syshandler_chdir(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	if (!is_user_ptr(path)) {
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

	t->cwd = n;
	return 0;
}
