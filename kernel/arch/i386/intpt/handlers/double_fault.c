#include "handlers.h"

void double_fault_handler(registers_t* regs) {
	kpanic("double fault\n  eip=0x%08x  ebp=0x%08x\n", regs->eip, regs->ebp);
}