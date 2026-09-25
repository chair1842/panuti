/* SPDX-License-Identifier: GPL-2.0-or-later */

// slab.h
#ifndef _KERNEL_MEMMAN_SLAB_H
#define _KERNEL_MEMMAN_SLAB_H

#include <stdint.h>

void* kmalloc(uint32_t size, uint32_t align);
void kfree(void* ptr);

#endif