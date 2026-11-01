/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "../../io.h"
#include <kernel/beepr.h>
#include <stdint.h>

#define PIT_CHANNEL2_PORT 0x42
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL2_MODE 0xB6

#define BEEPR_PORT 0x61
#define BEEPR_GATE 0x03

#define PIT_BASE_FREQ 1193182u
#define PIT_MAX_DIVISOR 0xFFFFu

int beepr_play(uint32_t frequency) {
	if (frequency == 0) {
		beepr_stop();
		return 0;
	}

	uint32_t divisor = PIT_BASE_FREQ / frequency;
	if (divisor == 0 || divisor > PIT_MAX_DIVISOR) {
		return -1;
	}

	outb(PIT_COMMAND_PORT, PIT_CHANNEL2_MODE);
	outb(PIT_CHANNEL2_PORT, (uint8_t)(divisor & 0xFF));
	outb(PIT_CHANNEL2_PORT, (uint8_t)(divisor >> 8));

	outb(BEEPR_PORT, inb(BEEPR_PORT) | BEEPR_GATE);
	return 0;
}

void beepr_stop(void) {
	outb(BEEPR_PORT, inb(BEEPR_PORT) & (uint8_t)~BEEPR_GATE);
}
