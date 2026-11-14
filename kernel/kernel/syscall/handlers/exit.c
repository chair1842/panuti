/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <panuti/errno.h>
#include <kernel/sched/task.h>
#include <stdint.h>

int32_t syshandler_exit(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;

	task_do_exit(a1);

	// unreachable: a terminated task is never rescheduled
	return PANUTIERRNO_PLAINSUCCESS;
}
