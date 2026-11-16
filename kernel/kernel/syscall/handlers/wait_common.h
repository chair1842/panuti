/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_SYSCALL_HANDLERS_WAIT_COMMON_H
#define _KERNEL_SYSCALL_HANDLERS_WAIT_COMMON_H

#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <kernel/mem/usr.h>
#include <kernel/irq.h>
#include <panuti/errno.h>
#include <stdbool.h>
#include <stdint.h>

static inline int32_t wait_commit_pids(task_t* caller, const pid_t* user_pids, uint32_t count) {
	if (count == 0 || count > TASK_WAIT_MAX) {
		return PANUTIERRNO_INVALIDARG;
	}

	if (!kernel_usrptr_verify(user_pids, count * sizeof(pid_t))) {
		return PANUTIERRNO_INVALIDADDR;
	}

	uint32_t flags = irq_save_disable();

	caller->wait_pid_count = 0;

	int32_t rc = PANUTIERRNO_PLAINSUCCESS;
	for (uint32_t i = 0; i < count; i++) {
		if (user_pids[i] == caller->pid) {
			rc = PANUTIERRNO_PLAINERR;
			break;
		}

		caller->wait_pids[i] = user_pids[i];
	}

	if (rc == PANUTIERRNO_PLAINSUCCESS) {
		caller->wait_pid_count = count;
	}

	irq_restore(flags);
	return rc;
}

static inline bool wait_outputs_valid(uint32_t ec_addr, uint32_t exited_addr) {
	return kernel_usrptr_verify((const void*)ec_addr, sizeof(int32_t)) &&
	       kernel_usrptr_verify((const void*)exited_addr, sizeof(pid_t));
}

#endif
