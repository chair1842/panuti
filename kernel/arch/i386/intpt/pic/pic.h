#ifndef ARCH_I386_INTPT_PIC_H 
#define ARCH_I386_INTPT_PIC_H

#include <stdint.h>

void pic_init(void);
void pic_send_eoi(uint8_t irq);

#endif