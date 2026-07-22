#include <kernel/sched/task.h>
#include <stdint.h>

extern void enter_usermode_trampoline(void);

void task_init_stack(task_t* t, void (*entry)(void)) {
	uint32_t* stack_top = (uint32_t*)(t->kernel_stack + TASK_KERNEL_STACK_SIZE);

	*(--stack_top) = (uint32_t)entry; // for ret to "return" to
	// fake ebp, ebx, esi, and edi
	*(--stack_top) = 0;
	*(--stack_top) = 0;
	*(--stack_top) = 0;
	*(--stack_top) = 0;

	t->esp = (uint32_t)stack_top;
}

void task_init_user_stack(task_t* t, void (*entry)(void), uint32_t user_esp) {
	uint32_t* stack_top = (uint32_t*)(t->kernel_stack + TASK_KERNEL_STACK_SIZE);

	*(--stack_top) = user_esp;
	*(--stack_top) = (uint32_t)entry;
	*(--stack_top) = (uint32_t)enter_usermode_trampoline;
	*(--stack_top) = 0; // ebp
	*(--stack_top) = 0; // ebx
	*(--stack_top) = 0; // esi
	*(--stack_top) = 0; // edi

	t->esp = (uint32_t)stack_top;
	t->user_stack = user_esp;
}