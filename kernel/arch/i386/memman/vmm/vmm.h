#ifndef ARCH_I386_MEMMAN_VMM_H
#define ARCH_I386_MEMMAN_VMM_H

#include <stdint.h>

void vmm_init(void);
void vmm_map(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
void vmm_unmap(uint32_t virt_addr);
uint32_t vmm_get_phys(uint32_t virt_addr);
/* Returns an opaque address-space handle containing the page directory's CR3 address. */
void* vmm_create_page_dir(void);
void* vmm_get_kernel_page_dir(void);

#endif
