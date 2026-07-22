#ifndef _KERNEL_SCHED_TASK_H
#define _KERNEL_SCHED_TASK_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/handle/handle.h>

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
	uint32_t user_stack;
	void* addr_space;
	task_state_t state;
	uint32_t pid;
	struct task* next;
	handle_t handles[MAX_HANDLES];
} task_t;

task_t* task_create(void (*entry)(void));
task_t* task_create_user(void (*entry)(void));
task_t* task_create_frelf_user(const void* elf_data, size_t elf_size);
void task_switch_to(task_t* old, task_t* new);
void task_init_stack(task_t* t, void (*entry)(void));
void task_init_user_stack(task_t* t, void (*entry)(void), uint32_t user_esp);
void task_activate(task_t* task);

#endif
