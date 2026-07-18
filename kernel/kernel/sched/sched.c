#include "kernel/klog.h"
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <stddef.h>
#include <stdbool.h>

static task_t* ready_queue = NULL;
static task_t* current = NULL;
static bool sched_initialized = false;

void sched_add(task_t* task) {
    if (!ready_queue) {
        ready_queue = task;
        task->next = task;
    } else {
        task->next = ready_queue->next;
        ready_queue->next = task;
        klog(KLOG_INFO, "sched_add: ready_queue (%x) next updated to %x\n", ready_queue->pid, ready_queue->next->pid);
    }

    klog(KLOG_INFO, "sched_add: next of task %x is %x\n", task->pid, task->next->pid);
}

void sched_schedule(void) {
	if (!sched_initialized) {
		return;
	}

	if (!current) {
		current = ready_queue;
		current->state = TASK_RUNNING;
		return;
	}

	task_t* prev = current;
	current = current->next;

	if (prev != current) {
		prev->state = TASK_READY;
		current->state = TASK_RUNNING;
		task_switch_to(prev, current);
	}

	klog(KLOG_INFO, "sched_schedule: switching from %p to %p\n", prev, current);
}

void sched_init(void) {
	if (!ready_queue) {
		return;
	}

	current = ready_queue;
	current->state = TASK_RUNNING;

	sched_initialized = true;

	task_t dummy; // to be thrownaway
	task_switch_to(&dummy, current);
}

task_t* sched_current(void) {
	return current;
}
