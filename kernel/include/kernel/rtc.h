/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_RTC_H
#define _KERNEL_RTC_H

#include <stdint.h>

int64_t rtc_cur_timestamp();

int rtc_set_timestamp(int64_t timestamp);

#endif