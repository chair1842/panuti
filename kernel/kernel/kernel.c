#include <kernel/sched/task.h>
#include <stdio.h>
#include <kernel/tty.h>
#include <kernel/sched/sched.h>
#include <kernel/klog.h>
#include <kernel/boot_mod.h>
#include <kernel/sched/task.h>
#include <kernel/kpanic.h>
#include "drivers/console/console.h"
#include <kernel/memman/tempmap.h>

void idle_task_entry(void) {
	while (1) {
	}
}

void kernel_main(void) {
	terminal_initialize();
	console_register();

	const void* elf_data;
	size_t elf_size;

	if (kernel_get_init_module(&elf_data, &elf_size) != 0) {
		kpanic("no init module available, halting init\n");
	}
	task_t* pint_task_1 = task_create_frelf_user(elf_data, elf_size);
	unmap_physical_temp((void*)elf_data, elf_size);

	if (kernel_get_init_module(&elf_data, &elf_size) != 0) {
		kpanic("no init module available, halting init\n");
	}
	task_t* pint_task_2 = task_create_frelf_user(elf_data, elf_size);
	unmap_physical_temp((void*)elf_data, elf_size);

	if (!pint_task_1 || !pint_task_2) {
		kpanic("failed to create pint test tasks\n");
	}
	klog(KLOG_INFO, "pint test tasks created, pid=%d and pid=%d\n", pint_task_1->pid, pint_task_2->pid);
	
	task_t* idle_task = task_create(idle_task_entry);
	if (!idle_task) {
		kpanic("failed to create idle task\n");
	}

	sched_init();
}