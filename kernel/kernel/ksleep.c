/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/timer.h>
#include <kernel/ksleep.h>
#include <kernel/sched/sched.h>
#include <kernel/irq.h>

void ksleep(uint32_t cs) {
	if (cs == 0) {
		sched_schedule();
		return;
	}

	uint32_t flags = irq_save_disable();

	task_t* t = sched_current();
	t->wake_at = timer_get_ticks() + cs + 1;
	task_block(t);

	irq_restore(flags);
}

void sleep_tick() {
	int64_t now = timer_get_ticks();
	task_t* t = sched_current();
	
	for (int i = 0; i < 64; i++) {
		if (t->wake_at && t->wake_at <= now) {
			t->wake_at = 0;
			task_wake(t);
		}
	}
}