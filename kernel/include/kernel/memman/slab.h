/* SPDX-License-Identifier: GPL-2.0-or-later */

// slab.h
#ifndef _KERNEL_MEMMAN_SLAB_H
#define _KERNEL_MEMMAN_SLAB_H

#include <stdint.h>

typedef struct {
	uint32_t slab_used;
	uint32_t slab_capacity;
	uint32_t slab_pages;
	uint32_t big_allocs;
	uint32_t big_pages;
	uint32_t big_alloc_max;
} slab_stats_t;

void* kmalloc(uint32_t size, uint32_t align);
void kfree(void* ptr);

void slab_get_stats(slab_stats_t* s);

#endif