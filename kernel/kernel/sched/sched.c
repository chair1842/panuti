#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <stddef.h>

static task_t* ready_queue = NULL;
static task_t* current = NULL;

void sched_add(task_t* task) {
    if (!ready_queue) {
        ready_queue = task;
        task->next = task;
    } else {
        task->next = ready_queue;
        ready_queue->next = task;
    }
}

void sched_schedule(void) {
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
}

void sched_init(void) {
	current = ready_queue;
	current->state = TASK_RUNNING;

	task_t dummy; // to be thrownaway
	task_switch_to(&dummy, current);
}

task_t* sched_current(void) {
	return current;
}