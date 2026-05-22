#include "handlers.h"

void page_fault_handler(registers_t* regs) {
	uint32_t cr2;
	__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

	uint8_t present  = regs->error & 0x1;
	uint8_t write    = (regs->error >> 1) & 0x1;
	uint8_t user     = (regs->error >> 2) & 0x1;
	uint8_t reserved = (regs->error >> 3) & 0x1;
	uint8_t ifetch   = (regs->error >> 4) & 0x1;

	printf("page fault at 0x%08x\n", cr2);
	printf("  %s | %s | %s\n",
		present ? "protection" : "non-present",
		write   ? "write"      : "read",
		user    ? "user"       : "kernel"
	);
	if (reserved) printf("  reserved bit set in PTE\n");
	if (ifetch)   printf("  instruction fetch\n");
	printf("  eip=0x%08x  error=0x%02x\n", regs->eip, regs->error);

	abort();
}