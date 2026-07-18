#include "main.h"
#include <stdlib.h>
#include "../pic/pic.h"

static isr_handler_t isr_handlers[256];

void register_handler(uint8_t vector, isr_handler_t handler) {
	isr_handlers[vector] = handler;
}

void isr_handler(registers_t* regs) {
	isr_handler_t handler = isr_handlers[regs->vector];

    /* A handler may switch tasks, so acknowledge the IRQ before dispatching it. */
    if (regs->vector >= 32 && regs->vector < 48) {
        pic_send_eoi(regs->vector - 32);
    }

    if (handler) {
        handler(regs);
    } else if (regs->vector < 32) {
        abort();
    }
}
