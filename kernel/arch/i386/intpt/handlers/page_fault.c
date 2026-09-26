/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "handlers.h"
#include <kernel/irq.h>
#include <kernel/klog.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>

// 128 + SIGSEGV, the shell convention for "died on signal N", so a parent that
// wait()s on a faulted child gets a status it can recognise
#define FAULT_EXIT_CODE 139

void page_fault_handler(registers_t* regs) {
	uint32_t cr2;
	__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

	uint8_t present  = regs->error & 0x1;
	uint8_t write    = (regs->error >> 1) & 0x1;
	uint8_t user     = (regs->error >> 2) & 0x1;
	uint8_t reserved = (regs->error >> 3) & 0x1;
	uint8_t ifetch   = (regs->error >> 4) & 0x1;

	klog(KLOG_WARN, "page fault at 0x%08x: %s, %s, from %s mode (eip=0x%08x error=0x%02x)\n",
		cr2,
		present ? "protection fault" : "unmapped page",
		write ? "write" : "read",
		user ? "user" : "kernel",
		regs->eip, regs->error);

	if (reserved) {
		klog(KLOG_WARN, "  reserved bit set in PTE\n");
	}
	if (ifetch) {
		klog(KLOG_WARN, "  fault was an instruction fetch\n");
	}
	
	if (ifetch) {
		klog(KLOG_WARN, "  fault was an instruction fetch");
	}
	
	if (!user) {
		kpanic("page fault in kernel mode at 0x%08x (eip=0x%08x)\n", cr2, regs->eip);
	}

	task_t* current = sched_current();
	if (!current) {
		kpanic("page fault at 0x%08x with no current task\n", cr2);
	}

	klog(KLOG_WARN, "  task %d: esp=0x%08x useresp=0x%08x ebp=0x%08x, killing it\n",
		current->pid, regs->esp, regs->useresp, regs->ebp);

	uint32_t flags = irq_save_disable();

	current->exit_code = FAULT_EXIT_CODE;
	current->state = TASK_TERMINATED;
	task_wake_waiters(current->pid);

	sched_schedule();

	irq_restore(flags);
	kpanic("page fault: sched_schedule returned on a terminated task\n");
}
