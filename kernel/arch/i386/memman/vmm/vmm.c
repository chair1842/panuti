#include "vmm.h"
#include "../pmm/pmm.h"
#include <string.h>

#define KERNEL_VIRT_OFFSET 0xC0000000
#define KERNEL_PH_V(addr) ((addr) + KERNEL_VIRT_OFFSET) // physical to virtual

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4

extern uint32_t boot_page_dir[1024];
extern uint32_t boot_page_table[1024];

static uint32_t* pte_get_table(uint32_t pde_entry) {
	// strip flags, return virtual address of page table
	return (uint32_t*)((pde_entry & ~0xFFF) + KERNEL_VIRT_OFFSET);
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

		uint32_t* pgtable_virt = (uint32_t*)(KERNEL_PH_V(pgtable_phys));
		memset(pgtable_virt, 0, 4096);

		uint32_t pde_flags = PAGE_PRESENT | PAGE_RW;
		if (flags & PAGE_USER) pde_flags |= PAGE_USER;

		boot_page_dir[pde_i] = pgtable_phys | pde_flags;
	}

	uint32_t* table = pte_get_table(boot_page_dir[pde_i]);
	table[pte_i] = phys_addr | flags | PAGE_PRESENT;

	__asm__ __volatile__("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

void vmm_unmap(uint32_t virt_addr) {
	uint32_t pde_i = virt_addr >> 22;
	uint32_t pte_i = (virt_addr >> 12) & 0b1111111111;

	if (!(boot_page_dir[pde_i] & PAGE_PRESENT)) {
		return;
	}

	uint32_t* table = pte_get_table(boot_page_dir[pde_i]);
	table[pte_i] = 0;

	__asm__ __volatile__("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

uint32_t vmm_get_phys(uint32_t virt_addr) {
	uint32_t pde_i = virt_addr >> 22;
	uint32_t pte_i = (virt_addr >> 12) & 0b1111111111;

	if (!(boot_page_dir[pde_i] & PAGE_PRESENT)) {
		return 0;
	}

	uint32_t* table = pte_get_table(boot_page_dir[pde_i]);
	if (!(table[pte_i] & PAGE_PRESENT)) {
		return 0;
	}

	return table[pte_i] & ~0xFFF;
}