#include <kernel/timer.h>
#include <stdint.h>
#include "drivers/pit/pit.h"

void timer_init(uint32_t freq) {
	pit_init(freq);
}

uint64_t timer_get_ticks(void) {
	return pit_get_ticks();
}