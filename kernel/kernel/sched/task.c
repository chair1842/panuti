#include <kernel/sched/task.h>
#include <kernel/memman/vmalloc.h>
#include <kernel/memman/memman.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_TASKS 64 

static uint32_t next_pid = 1;
static task_t tasks[MAX_TASKS] = {0};
static task_t* current = NULL;

task_t* task_create(void (*entry)(void)) {
	task_t* t = &tasks[next_pid];
	t->kernel_stack = (uint32_t)vmalloc_pg();
	t->pid = next_pid++;
	t->state = TASK_READY;
	t->addr_space = memman_create_addr_space();
	return t;
}
