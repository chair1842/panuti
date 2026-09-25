/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/irq.h>
#include <stdint.h>

uint32_t irq_save_disable(void) {
	uint32_t flags;
	__asm__ __volatile__("pushf\n\tpop %0\n\tcli" : "=r"(flags) :: "memory");
	return flags;
}

void irq_restore(uint32_t flags) {
	__asm__ __volatile__("push %0\n\tpopf" :: "r"(flags) : "memory", "cc");
}