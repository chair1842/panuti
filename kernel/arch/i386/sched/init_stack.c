#include <kernel/sched/task.h>
#include <stdint.h>

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
