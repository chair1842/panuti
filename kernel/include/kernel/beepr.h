/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_BEEPR_H
#define _KERNEL_BEEPR_H

#include <stdint.h>

int beepr_play(uint32_t frequency);
void beepr_stop(void);
void beepr_beep(void);
void beepr_tick(void);

#endif
