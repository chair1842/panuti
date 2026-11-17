/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_CPU_H
#define _KERNEL_CPU_H

// wait for the next interrupt. the idle task loops on this
void cpu_idle(void);

// disable interrupts and halt forever. used by the panic path
__attribute__((noreturn)) void cpu_halt(void);

#endif
