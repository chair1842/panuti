#include "pit.h"
#include "../../io.h"
#include <stdint.h>
#include "../../intpt/handlers/main.h"
#include <kernel/sched/sched.h>
#include <kernel/klog.h>
#include "../../intpt/pic/pic.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43
#define PIT_BASE_FREQ 1193182
#define PIT_MODE 0x36 // channel 0, lowbyte/hibyte, square wave generator, binary

#define PIT_VECTOR 32

static volatile uint64_t ticks = 0;

void pit_init(uint32_t freq) {
	if (freq == 0) {
		return;
	}

	if (freq > 1193182) {
		freq = 1193182;
	}
	
	uint16_t divisor = 1193182 / freq;
	if (divisor == 0) {
		divisor = 1;
	}

	// Set PIT to mode 2 (rate generator)
	outb(PIT_COMMAND, PIT_MODE);
	outb(PIT_CHANNEL0, divisor & 0xFF);	  // low byte
	outb(PIT_CHANNEL0, divisor >> 8);		// high byte

	register_handler(PIT_VECTOR, pit_handler);
}

uint64_t pit_get_ticks(void) {
	return ticks;
}

void pit_handler(registers_t *reg) {
	(void)reg;
	ticks++;
	klog(KLOG_INFO, "pit tick %llu\n", ticks);
	sched_schedule();
}
