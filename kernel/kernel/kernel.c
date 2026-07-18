#include "kernel/sched/task.h"
#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/sched/sched.h>
#include <kernel/klog.h>

void a(void) {
	asm volatile("sti");
	while (1) {
		klog(KLOG_INFO, "a\n");
		for (volatile int i = 0; i < 100000000; i++);
	}
}

void b(void) {
	asm volatile("sti");
	while (1) {
		klog(KLOG_INFO, "b\n");
		for (volatile int i = 0; i < 100000000; i++);
	}
}

void kernel_main(void) {
	terminal_initialize();
	printf("Panuti Kernel\n");
	
	task_create(a);
	task_create(b);	
	sched_init();
}
