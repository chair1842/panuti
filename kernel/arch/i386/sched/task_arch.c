#include <kernel/sched/task.h>
#include "../tss/tss.h"
#include "../msr.h"

/* Prepare i386 privilege-transition state for the task about to run. */
void task_activate(task_t* task) {
	uint32_t kstack_top = task->kernel_stack + TASK_KERNEL_STACK_SIZE;
	tss_set_kernel_stack(task->kernel_stack + TASK_KERNEL_STACK_SIZE);
	wrmsr32(MSR_SYSENTER_ESP, kstack_top);
}
