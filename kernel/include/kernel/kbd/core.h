/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_KBD_CORE_H
#define _KERNEL_KBD_CORE_H

#include <panuti/kbd.h>

void kbd_core_init(void);
void kbd_core_push(keypacket_t packet);

int kbd_core_read_line_queue(keypacket_t* out);
int kbd_core_read_raw_queue(keypacket_t* out);

#endif