#ifndef _KERNEL_MEMMAN_VMALLOC_H
#define _KERNEL_MEMMAN_VMALLOC_H
#include <stdint.h>
#include <stddef.h>

void vmalloc_init(void);
void* vmalloc_pg(void);

// Maps `npages` consecutive virtual pages backed by fresh physical frames.
// Returns the page-aligned base address, or NULL on failure.
void* vmalloc_pages(uint32_t npages);

// Frees a range previously returned by vmalloc_pages.
void vmalloc_free_pages(void* addr, uint32_t npages);

void vmalloc_free(void* addr);

#endif