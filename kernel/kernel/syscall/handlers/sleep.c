/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/sched/sched.h>
#include <kernel/syscall/handlers.h>
#include <kernel/ksleep.h>
#include <kernel/timer.h>
#include <stdint.h>

int32_t syshandler_sleep(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	uint32_t cs = a1;

	task_t* t = sched_current();
	uint32_t now = timer_get_ticks();

	ksleep(cs);

	return t->wake_at > now ? t->wake_at - now : 0;
}