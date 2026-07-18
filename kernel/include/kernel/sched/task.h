#ifndef _KERNEL_SCHED_TASK_H
#define _KERNEL_SCHED_TASK_H

#include <stdint.h>

#define TASK_KERNEL_STACK_SIZE 4096

typedef enum task_state {
	TASK_READY = 0,
	TASK_RUNNING = 1,
	TASK_BLOCKED = 2,
	TASK_TERMINATED = 3,
} task_state_t;

typedef struct task {
	uint32_t esp;
	uint32_t kernel_stack;
	/* Architecture-owned handle; the scheduler never inspects its contents. */
	void* addr_space;
	task_state_t state;
	uint32_t pid;
	struct task* next;
} task_t;

task_t* task_create(void (*entry)(void));
void task_switch_to(task_t* old, task_t* new);
void task_init_stack(task_t* t, void (*entry)(void));
void task_activate(task_t* task);

#endif
