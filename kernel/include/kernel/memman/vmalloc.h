/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_MEMMAN_VMALLOC_H
#define _KERNEL_MEMMAN_VMALLOC_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
	uint32_t capacity;
	uint32_t high_water;
	uint32_t in_use;
	uint32_t free_ranges;
} vmalloc_stats_t;

void vmalloc_init(void);
void* vmalloc_pg(void);

// Maps `npages` consecutive virtual pages backed by fresh physical frames.
// Returns the page-aligned base address, or NULL on failure.
void* vmalloc_pages(uint32_t npages);

// Frees a range previously returned by vmalloc_pages.
void vmalloc_free_pages(void* addr, uint32_t npages);

void vmalloc_free(void* addr);

void vmalloc_get_stats(vmalloc_stats_t* s);

#endif