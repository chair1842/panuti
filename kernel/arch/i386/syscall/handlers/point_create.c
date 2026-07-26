#include "handlers.h"
#include <kernel/handle/inode_type.h>
#include <kernel/handle/registry.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/point.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>

int32_t syshandler_point_create(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	if (!is_user_ptr(path)) {
		return PANUTIERRNO_INVALIDADDR;
	}
	
	task_t* t = sched_current();
	point_t* p = point_create(path, t);
	if (!p) {
		point_destroy(p);
		return PANUTIERRNO_PLAINERR;
	}

	int des = handle_alloc(t);
	if (des < 0) {
		point_destroy(p);
		return PANUTIERRNO_NOFDS;
	}
	
	inode_t* n = registry_resolve(t->cwd, path);
	if (!n) {
		point_destroy(p);
		return PANUTIERRNO_NOTFOUND;
	}
	
	if (n->type != INODE_POINT) {
		// and the point we just created had an identity crisis
		point_destroy(p);
		return PANUTIERRNO_PLAINERR;
	}
	
	t->handles[des].type = n->type;
	t->handles[des].impl = n->impl;
	t->handles[des].ops = n->ops;
	return des;
}