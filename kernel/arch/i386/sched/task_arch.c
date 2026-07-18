#include <kernel/sched/task.h>
#include "../tss/tss.h"

/* Prepare i386 privilege-transition state for the task about to run. */
void task_activate(task_t* task) {
	tss_set_kernel_stack(task->kernel_stack + TASK_KERNEL_STACK_SIZE);
}
