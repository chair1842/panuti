#include <kernel/sched/task.h>
#include <kernel/memman/vmalloc.h>
#include <kernel/memman/memman.h>
#include <stdint.h>
#include <stddef.h>
#include <kernel/sched/sched.h>

#define MAX_TASKS 64 

static uint32_t next_pid = 1;
static uint32_t task_count = 0;
static task_t tasks[MAX_TASKS] = {0};

task_t* task_create(void (*entry)(void)) {
	if (!entry || task_count == MAX_TASKS) {
		return NULL;
	}

	task_t* t = &tasks[task_count];
	t->kernel_stack = (uint32_t)vmalloc_pg();
	if (!t->kernel_stack) {
		return NULL;
	}

	t->pid = next_pid++;
	t->state = TASK_READY;
	t->addr_space = memman_create_addr_space();
	if (!t->addr_space) {
		vmalloc_free((void*)t->kernel_stack);
		return NULL;
	}
	
	task_init_stack(t, entry);
	sched_add(t);
	task_count++;
	
	return t;
}
