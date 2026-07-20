#include <kernel/sched/task.h>
#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/sched/sched.h>
#include <kernel/klog.h>
#include <kernel/memman/memman.h>
#include <kernel/memman/tempmap.h>

#include <kernel/elf.h>
#include "../arch/i386/memman/pmm/pmm.h"

extern task_t* task_create_frelf_user(const void* elf_data, size_t elf_size);

void kernel_main(void) {
	terminal_initialize();

	uint32_t module_size = init_module_phys_end - init_module_phys_start;
	const void* elf_data = map_physical_temp(init_module_phys_start, module_size);

	task_t* pint_task = task_create_frelf_user(elf_data, module_size);
	if (!pint_task) {
		klog(KLOG_INFO, "failed to create pint task from ELF\n");
	} else {
		klog(KLOG_INFO, "pint task created, pid=%d\n", pint_task->pid);
	}

	// NOTE: deliberately NOT unmapping elf_data here -- task_create_frelf_user
	// reads through it during elf_load_segments, which already completed by
	// the time this function returns, so it'd be safe to unmap now. Left
	// mapped for now only because nothing else needs that virtual range yet;
	// revisit once temp-map scratch space is reused elsewhere.

	sched_init();
}