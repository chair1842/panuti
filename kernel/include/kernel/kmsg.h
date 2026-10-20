/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_KMSG_H
#define _KERNEL_KMSG_H

#include <stddef.h>

void kmsg_push(int level, const char* data, size_t len);

void kmsg_init();

#endif
