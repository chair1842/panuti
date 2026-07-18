#include "vmm.h"
#include "../pmm/pmm.h"
#include <string.h>

#define KERNEL_VIRT_OFFSET 0xC0000000
#define RECURSIVE_TABLE_BASE 0xFFC00000
#define PAGE_SIZE 0x1000

/* A scratch page used while initializing a page directory not yet active in CR3. */
#define PAGE_DIR_TEMP_MAP 0xC0400000

#define KERNEL_PDE_START (KERNEL_VIRT_OFFSET >> 22)
#define RECURSIVE_PDE_INDEX 1023
#define PAGE_DIR_ENTRIES 1024

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
	uint32_t page_dir_phys = pmm_allocp();
	if (page_dir_phys == 0) {
		return NULL;
	}

	/*
	 * The new directory cannot be reached through its recursive entry until it
	 * becomes active.  Temporarily map its physical frame in the current
	 * kernel address space to initialize it.
	 */
	vmm_map(PAGE_DIR_TEMP_MAP, page_dir_phys, PAGE_RW);
	uint32_t* page_dir = (uint32_t*)PAGE_DIR_TEMP_MAP;
	memset(page_dir, 0, PAGE_SIZE);

	/* Every process shares the higher-half kernel mappings, never user space. */
	for (uint32_t i = KERNEL_PDE_START; i < RECURSIVE_PDE_INDEX; i++) {
		page_dir[i] = boot_page_dir[i];
	}

	/* Make the recursive mapping refer to this directory, not the kernel one. */
	page_dir[RECURSIVE_PDE_INDEX] = page_dir_phys | PAGE_PRESENT | PAGE_RW;
	vmm_unmap(PAGE_DIR_TEMP_MAP);

	/*
	 * An inactive directory has no stable virtual address.  Return its physical
	 * address as an opaque address-space handle; it is the value to load in CR3.
	 */
	return (void*)page_dir_phys;
}
