/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef ARCH_I386_INTPT_IDT_H
#define ARCH_I386_INTPT_IDT_H

// move idt_entry and idtr to implementation file bc not needed in interface.

void idt_init(void);

#endif