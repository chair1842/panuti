/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <kernel/irq.h>
#include <panuti/errno.h>
#include <stdint.h>

int32_t syshandler_kill(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	pid_t pid = (pid_t)a1;

	uint32_t flags = irq_save_disable();

	task_t* target = task_find(pid);
	if (!target) {
		irq_restore(flags);
		return PANUTIERRNO_NOTFOUND;
	}

	if (!target->killable) {
		irq_restore(flags);
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	if (target == sched_current()) {
		irq_restore(flags);
		task_do_exit(TASK_KILL_EXIT_CODE);
		return PANUTIERRNO_PLAINSUCCESS;
	}

	if (target->state == TASK_TERMINATED) {
		irq_restore(flags);
		return PANUTIERRNO_PLAINSUCCESS;
	}

	target->kill_pending = true;
	target->wake_at = 0;
	task_wake(target);

	irq_restore(flags);
	return PANUTIERRNO_PLAINSUCCESS;
}
