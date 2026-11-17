/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/cpu.h>

void cpu_idle(void) {
	__asm__ __volatile__("hlt");
}

__attribute__((noreturn)) void cpu_halt(void) {
	__asm__ __volatile__("cli");
	for (;;) {
		__asm__ __volatile__("hlt");
	}
}
