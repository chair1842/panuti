#include "main.h"
#include <stdlib.h>
#include "../pic/pic.h"

static isr_handler_t isr_handlers[256];

void register_idt_handler(uint8_t vector, isr_handler_t handler) {
	isr_handlers[vector] = handler;
}

void isr_handler(registers_t* regs) {
	isr_handler_t handler = isr_handlers[regs->vector];
    if (handler) {
        handler(regs);
    } else if (regs->vector < 32) {
        abort();
    }
	
	// unhandled irqs get ignored at the party.

    if (regs->vector >= 32 && regs->vector <= 47) {
        pic_send_eoi(regs->vector);
    }
}