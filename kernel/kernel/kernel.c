#include <kernel/sched/task.h>
#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/sched/sched.h>
#include <kernel/klog.h>
#include <kernel/memman/memman.h>

extern void (*test_payload_install(addr_space_t addr_space))(void);

void kernel_main(void) {
	terminal_initialize();
	printf("Panuti Kernel\n");

	task_t* ut = task_create_user(NULL);
	if (!ut) {
		klog(KLOG_INFO, "failed to create user task\n");
	} else {
		void (*entry)(void) = test_payload_install(ut->addr_space);
		task_init_user_stack(ut, entry, ut->user_stack);
	}

	sched_init();
}
