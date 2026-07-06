#include <kernel/memman/memman.h>
#include "pmm/pmm.h"
#include "vmm/vmm.h"

void memman_map(uint32_t virt, uint32_t phys, uint32_t flags) {
	vmm_map(virt, phys, flags);
}

void memman_unmap(uint32_t virt) {
	vmm_unmap(virt);
}

uint32_t memman_get_phys(uint32_t virt) {
	return vmm_get_phys(virt);
}

uint32_t memman_alloc_frame(void) {
	return pmm_allocp();
}

void memman_free_frame(uint32_t phys) {
	pmm_freep(phys);
}

void* memman_get_kernel_addr_space(void) {
	return vmm_get_kernel_page_dir();
}

void* memman_create_addr_space(void) {
	return vmm_create_page_dir();
}