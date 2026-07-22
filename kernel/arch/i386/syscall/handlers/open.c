#include "handlers.h"
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>

int32_t syshandler_open(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	// TODO: this trusts a raw user pointer — validate it's actually
	// mapped and user-accessible, and bound the string length, before
	// this goes anywhere near untrusted userspace code.

	registry_entry_t* e = registry_find(path);
	if (!e) {
		return PANUTIERRNO_NOTFOUND;
	}

	task_t* t = sched_current();
	int desc = handle_alloc(t);
	if (desc < 0) {
		return PANUTIERRNO_NOFDS;
	}

	t->handles[desc].type = e->type;
	t->handles[desc].impl = e->impl;
	t->handles[desc].ops = e->ops;
	return desc;
}