/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_IRQ_H
#define _KERNEL_IRQ_H

#include <stdint.h>

/*
 * Interrupt masking. This is the portable interface; the implementation is
 * arch-specific (see arch/<triplet>/irq.c).
 *
 * The PIT timer is the only preemption source (it calls sched_schedule on
 * every tick). Masking interrupts is therefore sufficient to make
 * check-then-block / state-change-then-wake sequences atomic with respect to
 * the scheduler, which is what the wait/wake protocols rely on.
 */

uint32_t irq_save_disable(void);
void irq_restore(uint32_t flags);

#endif