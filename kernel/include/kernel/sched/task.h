#ifndef _KERNEL_SCHED_TASK_H
#define _KERNEL_SCHED_TASK_H

#include <stdint.h>

typedef enum task_state {
	TASK_READY = 0,
	TASK_RUNNING = 1,
	TASK_BLOCKED = 2,
	TASK_TERMINATED = 3,
} task_state_t;

typedef struct task {
	uint32_t esp;
	uint32_t kernel_stack;
	uint32_t* addr_space;
	task_state_t state;
	uint32_t pid;
	struct task* next;
} task_t;

task_t* task_create(void (*entry)(void));

#endif