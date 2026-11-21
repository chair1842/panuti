/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "wait_common.h"

int32_t syshandler_waitp(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
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

	int exit_code;
	pid_t exited_pid;

	uint32_t flags = irq_save_disable();
	int state = task_wait_any(caller->wait_pids, caller->wait_pid_count, &exit_code, &exited_pid);
	caller->wait_pid_count = 0;
	irq_restore(flags);

	if (state < 0) {
		return PANUTIERRNO_NOTFOUND;
	}

	if (state > 0) {
		return PANUTIERRNO_BUSY;
	}

	*user_exit_code = exit_code;
	*user_exit_pid = exited_pid;
	return PANUTIERRNO_PLAINSUCCESS;
}
