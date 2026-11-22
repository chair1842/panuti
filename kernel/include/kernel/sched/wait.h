/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_SCHED_WAIT_H
#define _KERNEL_SCHED_WAIT_H

#include <kernel/sched/task.h>

typedef struct wait_entry {
	task_t* task;
	struct wait_entry* next;
} wait_entry_t;

typedef struct wait_queue {
	wait_entry_t* head;
} wait_queue_t;

static inline void wait_entry_init(wait_entry_t* entry, task_t* task) {
	entry->task = task;
	entry->next = nullptr;
}

static inline void wait_queue_init(wait_queue_t* q) {
	q->head = nullptr;
}

void wait_queue_add(wait_queue_t* q, wait_entry_t* entry);
void wait_queue_remove(wait_queue_t* q, wait_entry_t* entry);
void wait_queue_wake_all(wait_queue_t* q);
void wait_queue_wake_one(wait_queue_t* q);

#endif
