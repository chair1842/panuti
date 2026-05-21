#ifndef ARCH_I386_INTPT_HANDLERS_H
#define ARCH_I386_INTPT_HANDLERS_H

#include "main.h"
#include <stdlib.h>
#include <stdio.h>

void page_fault_handler(registers_t* regs);
void gpf_handler(registers_t* regs);
void double_fault_handler(registers_t* regs);
void dvbz_handler(registers_t* regs);
void invalid_opcode_handler(registers_t* regs);

#endif