/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/registry.h>
#include <kernel/handle/handle.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include <kernel/syscall/handlers.h>

int32_t syshandler_close(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	int desc = (int)a1;

	task_t* t = sched_current();
	handle_t* h = task_return_hslot((uint32_t)desc, t);
	if (!h) {
		return PANUTIERRNO_BADFD;
	}

	inode_unref(h->inode);
	int ret = h->ops->close(h->impl, t);
	handle_free(t, desc);
	return ret;
}
