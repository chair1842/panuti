/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_TIMEDEV_H
#define _KERNEL_TIMEDEV_H

#include <stdint.h>

void timedev_uptime_register();
void timedev_boot_register();
void timedev_clock_register();

void timedev_init_coord();
int64_t timedev_get_bt();

#endif