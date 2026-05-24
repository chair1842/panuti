#ifndef _KERNEL_MEMMAN_H
#define _KERNEL_MEMMAN_H

#include <stdint.h>

#define MEMMAN_PRESENT 0x1
#define MEMMAN_RW 0x2

void memman_map(uint32_t virt, uint32_t phys, uint32_t flags);
void memman_unmap(uint32_t virt);
uint32_t memman_get_phys(uint32_t virt);
uint32_t memman_alloc_frame(void);
void memman_free_frame(uint32_t phys);

#endif