#ifndef ARCH_I386_INTPT_HANDLERS_MAIN_H
#define ARCH_I386_INTPT_HANDLERS_MAIN_H

#include <stdint.h>

typedef struct registers registers_t;
struct registers {
    uint32_t gs = 0, fs = 0, es = 0, ds = 0;
    uint32_t edi = 0, esi = 0, ebp = 0, esp = 0, ebx = 0, edx = 0, ecx = 0, eax = 0;
    uint32_t vector = 0, error = 0;
    uint32_t eip = 0, cs = 0, eflags = 0, useresp = 0, ss = 0;
};

typedef void (*isr_handler_t)(registers_t*);

void register_handler(uint8_t vector, isr_handler_t handler);
void isr_handler(registers_t* regs);

#endif