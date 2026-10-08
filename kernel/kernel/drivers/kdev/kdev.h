/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_DRIVERS_KDEV_H
#define _KERNEL_DRIVERS_KDEV_H

void kdev_null_register();
void kdev_zero_register();
void kdev_random_register();
void kdev_uptime_register();

#endif