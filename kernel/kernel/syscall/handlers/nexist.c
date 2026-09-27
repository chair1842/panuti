/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <kernel/handle/registry.h>
#include <kernel/mem/usr.h>
#include <panuti/errno.h>
#include <stdint.h>

int32_t syshandler_nexist(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	if (!kernel_is_user_ptr(path) || kernel_user_strlen(path) == (size_t)-1) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();

	// whatever produced the entry does not matter, a path resolves to
	// something whether it came out of the native registry tree or out of a
	// mounted filesystem. note that error codes live above 0x80000000, so 0 and
	// 1 are free to mean "no" and "yes" here.
	return registry_resolve(t->cwd, path) ? 1 : 0;
}
