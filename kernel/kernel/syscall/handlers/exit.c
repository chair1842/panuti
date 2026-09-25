/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <panuti/errno.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <kernel/irq.h>
#include <stdint.h>

int32_t syshandler_exit(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	uint32_t exit_code = a1;

	task_t* current = sched_current();

	// Keep the terminated-state change and the waiter wake atomic w.r.t. the
	// timer: if we were preempted in between, a waiting parent could reap us
	// (freeing this task's stack) while we are still running this syscall.
	uint32_t flags = irq_save_disable();

	current->exit_code = exit_code;
	current->state = TASK_TERMINATED;
	task_wake_waiters(current->pid);
	sched_schedule();

	// unreachable: a terminated task is never rescheduled
	irq_restore(flags);
	return PANUTIERRNO_PLAINSUCCESS;
}