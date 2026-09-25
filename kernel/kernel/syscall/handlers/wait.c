/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <kernel/sched/sched.h>
#include <kernel/irq.h>
#include <panuti/errno.h>

int32_t syshandler_wait(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3; (void)a4;
	pid_t target = (pid_t)a1;
	int* user_exit_code = (int*)a2;

	if (!kernel_is_user_range(user_exit_code, sizeof(int))) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* caller = sched_current();

	if (target == caller->pid) {
		return PANUTIERRNO_PLAINERR;
	}

	while (1) {
		int exit_code;

		// The only preemption source is the PIT timer, so disabling
		// interrupts makes the status check + task_block atomic: an exiting
		// child cannot run (and miss our block) in between. Registers us as a
		// waiter *before* the check so a child that already exited is reaped,
		// and one that exits after this point finds us blocked.
		uint32_t flags = irq_save_disable();
		caller->pid_waiting_on = target;

		int rc = task_wait_pid(target, &exit_code);

		if (rc == -1) {
			caller->pid_waiting_on = 0;
			irq_restore(flags);
			return PANUTIERRNO_NOTFOUND;
		}
		if (rc == 0) {
			caller->pid_waiting_on = 0;
			irq_restore(flags);
			*user_exit_code = exit_code;
			return PANUTIERRNO_PLAINSUCCESS;
		}

		// rc == 1: still running, block and retry. task_block switches away
		// with interrupts disabled; each task resumes with its own saved
		// EFLAGS, so irq_restore() re-enables them once we're back.
		task_block(caller);
		irq_restore(flags);
	}
}