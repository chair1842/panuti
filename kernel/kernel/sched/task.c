#include <kernel/sched/task.h>
#include <kernel/memman/vmalloc.h>
#include <kernel/memman/memman.h>
#include <stdint.h>
#include <stddef.h>
#include <kernel/sched/sched.h>

#define MAX_TASKS 64
#define USER_STACK_VIRT_TOP 0xB0000000
#define PAGE_SIZE 0x1000

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

task_t* task_create_user(void (*entry)(void)) {
	if (task_count == MAX_TASKS) {
		return NULL;
	}
	task_t* t = &tasks[task_count];

	t->kernel_stack = (uint32_t)vmalloc_pg();
	if (!t->kernel_stack) {
		return NULL;
	}

	t->addr_space = memman_create_addr_space();
	if (!t->addr_space) {
		vmalloc_free((void*)t->kernel_stack);
		return NULL;
	}

	uint32_t user_stack_phys = memman_alloc_frame();
	if (!user_stack_phys) {
		vmalloc_free((void*)t->kernel_stack);
		return NULL;
	}

	uint32_t user_stack_virt_base = USER_STACK_VIRT_TOP - PAGE_SIZE;
	memman_map_in(t->addr_space, user_stack_virt_base, user_stack_phys, MEMMAN_PRESENT | MEMMAN_RW | MEMMAN_USER);
	uint32_t user_esp = user_stack_virt_base + PAGE_SIZE;

	t->pid = next_pid++;
	t->state = TASK_READY;

	task_init_user_stack(t, entry, user_esp);
	sched_add(t);
	task_count++;

	return t;
}