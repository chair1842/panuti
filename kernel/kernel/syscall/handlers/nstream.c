/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "kernel/mem/usr.h"
#include "kernel/sched/sched.h"
#include <kernel/syscall/handlers.h>
#include <panuti/errno.h>

int32_t syshandler_nstream(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	uint32_t* out = (uint32_t*)a1;

	if (!kernel_is_user_range(out, 2 * sizeof(uint32_t))) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();

	out[0] = t->no_in_streams;
	out[1] = t->no_out_streams;

	return 0;
}