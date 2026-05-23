#include "vmalloc.h"
#include <stdint.h>
#include "../vmm/vmm.h"
#include "../pmm/pmm.h"

#define PAGE_ALIGN_UP(addr) (((addr) + 0xFFF) & ~0xFFF)

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2

extern uint32_t _kernel_end;
static uint32_t vmalloc_next;

void vmalloc_init(void) {
	vmalloc_next = PAGE_ALIGN_UP((uint32_t)&_kernel_end);
}

void* vmalloc_pg(void) {
	uint32_t phys = pmm_allocp();
	if (phys == 0) {
		return 0;
	}

	uint32_t virt = vmalloc_next;
	vmalloc_next += 4096;

	vmm_map(virt, phys, PAGE_PRESENT | PAGE_RW);
	return (void*)virt;
}

void vmalloc_free(void* addr) {
	uint32_t virt = (uint32_t)addr;
	uint32_t phys = vmm_get_phys(virt);
	if (phys == 0) {
		return;
	}

	vmm_unmap(virt);
	pmm_freep(phys);
}