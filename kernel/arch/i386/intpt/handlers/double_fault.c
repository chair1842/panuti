#include "handlers.h"

void double_fault_handler(registers_t* regs) {
	printf("double fault\n");
	printf("  eip=0x%08x  esp=0x%08x\n", regs->eip, regs->ebp);
	abort();
}