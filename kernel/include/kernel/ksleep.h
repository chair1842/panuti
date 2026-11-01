/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_KSLEEP_H
#define _KERNEL_KSLEEP_H

#include <stdint.h>

void sleep_tick();
void ksleep(uint32_t cs);

#endif