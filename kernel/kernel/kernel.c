#include <kernel/handle/registry.h>
#include <kernel/sched/task.h>
#include <stdio.h>
#include <kernel/tty.h>
#include <kernel/sched/sched.h>
#include <kernel/klog.h>
#include <kernel/boot_mod.h>
#include <kernel/kpanic.h>
#include <kernel/ata/atapi.h>
#include "drivers/vga/vga.h"
#include "drivers/ramblock/ramblock.h"

static void idle_task_entry(void) {
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

void kernel_main(void) {
	registry_init();
	terminal_initialize();
	
	registry_mkdir("/dvc");
	vga_register_console();
	ramblock_init("/dvc/ram0", 512, 1024);
	atapi_init();

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