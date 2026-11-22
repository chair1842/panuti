/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>
#include <kernel/handle/handle.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/wait.h>
#include <kernel/irq.h>
#include <kernel/timer.h>
#include <panuti/errno.h>
#include <stdint.h>
#include <string.h>

#define HWAIT_MAX_FDS 32

int32_t syshandler_hwait(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	const int* user_fds = (const int*)a1;
	uint32_t* which = (uint32_t*)a2;
	uint32_t n = a3;
	int32_t timeout_cs = (int32_t)a4;

	if (n == 0 || n > HWAIT_MAX_FDS || timeout_cs < -1) {
		return PANUTIERRNO_INVALIDARG;
	}

	if (!user_fds || !which || !kernel_usrptr_verify(user_fds, sizeof(int) * (size_t)n) || !kernel_usrptr_verify(which, sizeof(uint32_t))) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();
	if (!t) {
		return PANUTIERRNO_PLAINERR;
	}

	int fds[HWAIT_MAX_FDS];
	memcpy(fds, user_fds, sizeof(int) * (size_t)n);

	for (uint32_t i = 0; i < n; i++) {
		for (uint32_t j = i + 1; j < n; j++) {
			if (fds[i] == fds[j]) {
				return PANUTIERRNO_INVALIDARG;
			}
		}
	}

	handle_t* handles[HWAIT_MAX_FDS];
	wait_queue_t* queues[HWAIT_MAX_FDS];
	wait_entry_t entries[HWAIT_MAX_FDS];

	for (uint32_t i = 0; i < n; i++) {
		handles[i] = task_return_hslot((uint32_t)fds[i], t);
		if (!handles[i]) {
			return PANUTIERRNO_BADFD;
		}

		if (!handles[i]->ops || !handles[i]->ops->ready || !handles[i]->ops->waitq) {
			return PANUTIERRNO_UNSUPPORTEDOP;
		}

		queues[i] = handles[i]->ops->waitq(handles[i]->impl);
		if (!queues[i]) {
			return PANUTIERRNO_UNSUPPORTEDOP;
		}
	}

	uint32_t flags = irq_save_disable();
	t->wake_at = 0;

	int64_t deadline = 0;
	if (timeout_cs > 0) {
		deadline = (int64_t)timer_get_ticks() + timeout_cs + 1;
		t->wake_at = deadline;
	} else if (timeout_cs == 0) {
		deadline = (int64_t)timer_get_ticks();
	}

	bool blocking = timeout_cs != 0;
	if (blocking) {
		for (uint32_t i = 0; i < n; i++) {
			wait_entry_init(&entries[i], t);
			wait_queue_add(queues[i], &entries[i]);
		}
	}

	int32_t rc = PANUTIERRNO_PLAINERR;
	uint32_t ready_index = 0;

	while (1) {
		for (uint32_t i = 0; i < n; i++) {
			int ready = handles[i]->ops->ready(handles[i]->impl);
			if (ready < 0) {
				rc = PANUTIERRNO_UNSUPPORTEDOP;
				goto out;
			}

			if (ready > 0) {
				ready_index = i;
				rc = PANUTIERRNO_PLAINSUCCESS;
				goto out;
			}
		}

		if (t->kill_pending) {
			rc = PANUTIERRNO_PLAINERR;
			goto out;
		}

		if (timeout_cs >= 0 && (int64_t)timer_get_ticks() >= deadline) {
			rc = PANUTIERRNO_TIMEDOUT;
			goto out;
		}

		task_block(t);
	}

out:
	if (blocking) {
		for (uint32_t i = 0; i < n; i++) {
			wait_queue_remove(queues[i], &entries[i]);
		}
	}

	t->wake_at = 0;
	irq_restore(flags);

	if (rc == PANUTIERRNO_PLAINSUCCESS) {
		*which = ready_index;
	}

	return rc;
}
