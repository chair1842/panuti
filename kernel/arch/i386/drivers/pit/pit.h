#ifndef ARCH_I386_DRIVERS_PIT_H
#define ARCH_I386_DRIVERS_PIT_H

#include <stdint.h>
#include "../../intpt/handlers/main.h"

void pit_init(uint32_t freq);
uint64_t pit_get_ticks(void);
void pit_handler(registers_t* reg);

#endif