/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_SCHED_WAIT_H
#define _KERNEL_SCHED_WAIT_H

#include <kernel/sched/task.h>

typedef struct wait_queue {
	task_t* head;
} wait_queue_t;

static inline void wait_queue_init(wait_queue_t* q) {
	q->head = nullptr;
}

void wait_queue_add(wait_queue_t* q, task_t* t);
void wait_queue_remove(wait_queue_t* q, task_t* t);
void wait_queue_wake_all(wait_queue_t* q);
void wait_queue_wake_one(wait_queue_t* q);

#endif
