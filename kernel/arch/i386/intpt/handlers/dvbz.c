#include "handlers.h"

// divide by zero handler
void dvbz_handler(registers_t* regs) {
	printf("divide by zero\n");
	printf("  eip=0x%08x\n", regs->eip);
	abort();
}