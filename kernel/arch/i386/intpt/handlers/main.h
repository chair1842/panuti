#ifndef ARCH_I386_INTPT_HANDLERS_MAIN_H
#define ARCH_I386_INTPT_HANDLERS_MAIN_H

#include <stdint.h>

typedef struct registers registers_t;
struct registers {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t vector, error;
    uint32_t eip, cs, eflags, useresp, ss;
};

void isr_handler(registers_t* regs);

#endif