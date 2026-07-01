#ifndef _KERNEL_TIMER_H
#define _KERNEL_TIMER_H

#include <stdint.h>

void timer_init(uint32_t freq);
uint64_t timer_get_ticks(void);

#endif