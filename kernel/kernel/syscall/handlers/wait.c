/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "wait_common.h"

int32_t syshandler_wait(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	int32_t* user_exit_code = (int32_t*)a3;
	pid_t* user_exit_pid = (pid_t*)a4;

	if (!wait_outputs_valid(a3, a4)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* caller = sched_current();
	if (!caller) {
		return PANUTIERRNO_PLAINERR;
	}

	int32_t rc = wait_commit_pids(caller, (const pid_t*)a1, a2);
	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		return rc;
	}

	while (1) {
		int exit_code;
		pid_t exited_pid;

		uint32_t flags = irq_save_disable();

		if (caller->kill_pending) {
			caller->wait_pid_count = 0;
			irq_restore(flags);
			return PANUTIERRNO_PLAINERR;
		}

		int state = task_wait_any(caller->wait_pids, caller->wait_pid_count, &exit_code, &exited_pid);

		if (state == 0) {
			caller->wait_pid_count = 0;
			irq_restore(flags);
			*user_exit_code = exit_code;
			*user_exit_pid = exited_pid;
			return PANUTIERRNO_PLAINSUCCESS;
		}

		if (state < 0) {
			caller->wait_pid_count = 0;
			irq_restore(flags);
			return PANUTIERRNO_NOTFOUND;
		}

		task_block(caller);

		if (caller->kill_pending) {
			caller->wait_pid_count = 0;
			irq_restore(flags);
			return PANUTIERRNO_PLAINERR;
		}

		irq_restore(flags);
	}
}
