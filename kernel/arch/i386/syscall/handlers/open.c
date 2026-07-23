#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include <stdint.h>

int32_t syshandler_open(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	// TODO: unvalidated user pointer
	// path needs bounds/mapping checks before this is safe against
	// untrusted userspace.

	task_t* t = sched_current();

	inode_t* n = registry_resolve(t->cwd, path);
	if (!n) {
		return PANUTIERRNO_NOTFOUND;
	}
	if (n->type == INODE_DIR) {
		return PANUTIERRNO_UNSUPPORTEDOP; // no directory-open semantics yet
	}

	int fd = handle_alloc(t);
	if (fd < 0) {
		return PANUTIERRNO_NOFDS;
	}

	t->handles[fd].type = n->type;
	t->handles[fd].impl = n->impl;
	t->handles[fd].ops = n->ops;
	return fd;
}