#include "handlers.h"

void invalid_opcode_handler(registers_t* regs) {
	kpanic("invalid opcode at 0x%08x\n", regs->eip);
}