#include "vmm.h"
#include "../pmm/pmm.h"
#include <string.h>

#define KERNEL_VIRT_OFFSET 0xC0000000
#define RECURSIVE_TABLE_BASE 0xFFC00000

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2

extern uint32_t boot_page_dir[1024];
extern uint32_t boot_page_table[1024];

static uint32_t* pte_get_table(uint32_t pde_i) {
	return (uint32_t*)(RECURSIVE_TABLE_BASE + pde_i * 0x1000);
}

void vmm_init(void) {
	boot_page_dir[0] = 0;

	__asm__ __volatile__("mov %%cr3, %%eax\n" "mov %%eax, %%cr3\n" ::: "eax");
}

void vmm_map(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
	uint32_t pde_i = virt_addr >> 22;
	uint32_t pte_i = (virt_addr >> 12) & 0b1111111111;

	if (!(boot_page_dir[pde_i] & PAGE_PRESENT)) {
		uint32_t pgtable_phys = pmm_allocp();
		if (pgtable_phys == 0) {
			return;
		}

		boot_page_dir[pde_i] = pgtable_phys | PAGE_PRESENT | PAGE_RW;
		uint32_t* pgtable_virt = pte_get_table(pde_i);
		__asm__ __volatile__("invlpg (%0)" :: "r"(pgtable_virt) : "memory");
		memset(pgtable_virt, 0, 4096);
	}

	uint32_t* table = pte_get_table(pde_i);
	table[pte_i] = phys_addr | flags | PAGE_PRESENT;

	__asm__ __volatile__("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

void vmm_unmap(uint32_t virt_addr) {
	uint32_t pde_i = virt_addr >> 22;
	uint32_t pte_i = (virt_addr >> 12) & 0b1111111111;

	if (!(boot_page_dir[pde_i] & PAGE_PRESENT)) {
		return;
	}

	uint32_t* table = pte_get_table(pde_i);
	table[pte_i] = 0;

	__asm__ __volatile__("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

uint32_t vmm_get_phys(uint32_t virt_addr) {
	uint32_t pde_i = virt_addr >> 22;
	uint32_t pte_i = (virt_addr >> 12) & 0b1111111111;

	if (!(boot_page_dir[pde_i] & PAGE_PRESENT)) {
		return 0;
	}

	uint32_t* table = pte_get_table(pde_i);
	if (!(table[pte_i] & PAGE_PRESENT)) {
		return 0;
	}

	return table[pte_i] & ~0xFFF;
}

void* vmm_get_kernel_page_dir(void) {
	return boot_page_dir;
}

void* vmm_create_page_dir(void) {
	return vmm_get_kernel_page_dir(); // todo : dont forget to change this when you need to.
}
