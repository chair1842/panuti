/* SPDX-License-Identifier: GPL-2.0-or-later */

// coord keeps track of boot time

#include "time.h"
#include <kernel/rtc.h>

static int64_t boott = 0; 

void timedev_init_coord() {
	boott = rtc_cur_timestamp();
}

int64_t timedev_get_bt() {
	return boott;
}