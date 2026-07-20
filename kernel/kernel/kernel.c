#include <kernel/sched/task.h>
#include <stdio.h>
#include <kernel/tty.h>
#include <kernel/sched/sched.h>
#include <kernel/klog.h>
#include <kernel/boot_mod.h>
#include <kernel/sched/task.h>

void kernel_main(void) {
	terminal_initialize();

	const void* elf_data;
	size_t elf_size;
	if (kernel_get_init_module(&elf_data, &elf_size) != 0) {
		klog(KLOG_INFO, "no init module available, halting init\n");
		sched_init();
		return;
	}

	task_t* pint_task = task_create_frelf_user(elf_data, elf_size);
	if (!pint_task) {
		klog(KLOG_INFO, "failed to create pint task from ELF\n");
	} else {
		klog(KLOG_INFO, "pint task created, pid=%d\n", pint_task->pid);
	}

	sched_init();
}