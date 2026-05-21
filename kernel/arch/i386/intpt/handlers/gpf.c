#include "handlers.h"

void gpf_handler(registers_t* regs) {
	printf("general protection fault\n");
	printf("  error=0x%04x", regs->error);

	if (regs->error & 0x1) {
		uint8_t table = (regs->error >> 1) & 0x3;
		uint16_t index = (regs->error >> 3) & 0x1fff;
		const char* table_name[] = { "GDT", "IDT", "LDT", "IDT" };
		printf(" (selector: %s[%d])", table_name[table], index);
	}

	printf("\n  eip=0x%08x  cs=0x%04x\n", regs->eip, regs->cs);
	printf("  eax=0x%08x  ebx=0x%08x  ecx=0x%08x  edx=0x%08x\n",
		regs->eax, regs->ebx, regs->ecx, regs->edx);
	printf("  esi=0x%08x  edi=0x%08x  ebp=0x%08x\n",
		regs->esi, regs->edi, regs->ebp);

	abort();
}