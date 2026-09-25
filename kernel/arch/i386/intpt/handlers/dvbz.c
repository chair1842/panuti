/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "handlers.h"

// divide by zero handler
void dvbz_handler(registers_t* regs) {
	kpanic("divide by zero\n  eip=0x%08x\n", regs->eip);
}