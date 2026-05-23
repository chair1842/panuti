#ifndef ARCH_I386_MEMMAN_VMM_H
#define ARCH_I386_MEMMAN_VMM_H

#include <stdint.h>

void vmm_init(void);
void vmm_map(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
void vmm_unmap(uint32_t virt_addr);
uint32_t vmm_get_phys(uint32_t virt_addr);

#endif