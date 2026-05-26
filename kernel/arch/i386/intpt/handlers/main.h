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

typedef void (*isr_handler_t)(registers_t*);

void register_idt_handler(uint8_t vector, isr_handler_t handler);
void isr_handler(registers_t* regs);

#endif