#ifndef _KERNEL_MEMMAN_H
#define _KERNEL_MEMMAN_H

#include <stdint.h>

#define MEMMAN_PRESENT 0x1
#define MEMMAN_RW 0x2
#define MEMMAN_USER 0x4

typedef void* addr_space_t;

void memman_map(uint32_t virt, uint32_t phys, uint32_t flags);
void memman_unmap(uint32_t virt);
void memman_map_in(addr_space_t addr_space, uint32_t virt, uint32_t phys, uint32_t flags);
void memman_unmap_in(addr_space_t addr_space, uint32_t virt);
uint32_t memman_get_phys(uint32_t virt);
uint32_t memman_alloc_frame(void);
void memman_free_frame(uint32_t phys);
addr_space_t memman_create_addr_space(void);
addr_space_t memman_get_kernel_addr_space(void);

#endif
