#include <kernel/sched/task.h>
#include <stdio.h>
#include <kernel/tty.h>
#include <kernel/sched/sched.h>
#include <kernel/klog.h>
#include <kernel/boot_mod.h>
#include <kernel/sched/task.h>
#include <kernel/kpanic.h>
#include "drivers/vga/vga.h"

static void idle_task_entry(void) {
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

void kernel_main(void) {
	terminal_initialize();
	vga_register_console();

	const void* elf_data;
	size_t elf_size;
	if (kernel_get_init_module(&elf_data, &elf_size) != 0) {
		kpanic("no init module available, halting init\n");
	}

	task_t* pint_task = task_create_frelf_user(elf_data, elf_size);
	if (!pint_task) {
		kpanic("failed to create pint task from ELF\n");
	} else {
		klog(KLOG_INFO, "pint task created, pid=%d\n", pint_task->pid);
	}
	
	task_t* idle_task = task_create(idle_task_entry);
	if (!idle_task) {
		kpanic("failed to create idle task\n");
	}

	sched_init();
}