#include "kernel/sched/task.h"
#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/sched/sched.h>
#include <kernel/klog.h>

void a(void) {
	while (1) {
		klog(KLOG_INFO, "a\n");
		sched_schedule();
	}
}

void b(void) {
	while (1) {
		klog(KLOG_INFO, "b\n");
		sched_schedule();
	}
}

void kernel_main(void) {
	terminal_initialize();
	printf("Panuti Kernel\n");
	
	task_create(a);
	task_create(b);	
	sched_init();
}
