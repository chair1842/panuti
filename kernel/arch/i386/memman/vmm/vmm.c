/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "vmm.h"
#include "../pmm/pmm.h"
#include <string.h>
#include <kernel/klog.h>
#include <kernel/irq.h>
#include <kernel/mem/usr.h>

#define KERNEL_VIRT_OFFSET 0xC0000000
#define RECURSIVE_TABLE_BASE 0xFFC00000
#define PAGE_SIZE 0x1000

/* A scratch page used while initializing a page directory not yet active in CR3.
 * Lives above the vmalloc heap (which is capped at 0xC0800000). */
#define PAGE_DIR_TEMP_MAP 0xC0800000

#define KERNEL_PDE_START (KERNEL_VIRT_OFFSET >> 22)
#define RECURSIVE_PDE_INDEX 1023
#define PAGE_DIR_ENTRIES 1024

// top of the vmalloc heap window; must match VMALLOC_END in vmalloc.c
#define KERNEL_HEAP_END 0xC0800000

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4

extern uint32_t boot_page_dir[1024];
extern uint32_t boot_page_table[1024];

static uint32_t* pte_get_table(uint32_t pde_i) {
	return (uint32_t*)(RECURSIVE_TABLE_BASE + pde_i * 0x1000);
}

static uint32_t* current_page_dir(void) {
	return (uint32_t*)(RECURSIVE_TABLE_BASE + RECURSIVE_PDE_INDEX * PAGE_SIZE);
}

static uint32_t read_cr3(void) {
	uint32_t value;
	__asm__ __volatile__("mov %%cr3, %0" : "=r"(value));
	return value;
}

static void write_cr3(uint32_t value) {
	__asm__ __volatile__("mov %0, %%cr3" :: "r"(value) : "memory");
}

void vmm_init(void) {
	boot_page_dir[0] = 0;

	/*
	 * boot.S only maps the first 4 MB of the higher half (PDE 768). vmalloc
	 * grows past that into PDE 769+ (0xc0400000..KERNEL_HEAP_END), where
	 * vmm_map would lazily allocate the page table into whichever directory
	 * is active at the time. Task directories snapshot the kernel PDEs once
	 * in vmm_create_page_dir(), so a lazily created table would be visible
	 * only to tasks cloned after that point -- everything older would then
	 * fault on kmalloc() touching a slab beyond the stale PDE. Pre-create
	 * these tables now, before any task exists, so all address spaces share
	 * the same kernel heap mappings.
	 */
	for (uint32_t pde_i = KERNEL_PDE_START; pde_i < (KERNEL_HEAP_END >> 22); pde_i++) {
		if (boot_page_dir[pde_i] & PAGE_PRESENT) {
			continue;
		}
		uint32_t pgtable_phys = pmm_allocp();
		if (pgtable_phys == 0) {
			break;
		}
		boot_page_dir[pde_i] = pgtable_phys | PAGE_PRESENT | PAGE_RW;
		uint32_t* pgtable_virt = pte_get_table(pde_i);
		__asm__ __volatile__("invlpg (%0)" :: "r"(pgtable_virt) : "memory");
		memset(pgtable_virt, 0, PAGE_SIZE);
	}

	__asm__ __volatile__("mov %%cr3, %%eax\n" "mov %%eax, %%cr3\n" ::: "eax");
}

void vmm_map(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
	uint32_t pde_i = virt_addr >> 22;
	uint32_t pte_i = (virt_addr >> 12) & 0b1111111111;
	uint32_t* page_dir = current_page_dir();

	if (!(page_dir[pde_i] & PAGE_PRESENT)) {
		uint32_t pgtable_phys = pmm_allocp();
		if (pgtable_phys == 0) {
			return;
		}

		page_dir[pde_i] = pgtable_phys | PAGE_PRESENT | PAGE_RW | (flags & PAGE_USER);
		uint32_t* pgtable_virt = pte_get_table(pde_i);
		__asm__ __volatile__("invlpg (%0)" :: "r"(pgtable_virt) : "memory");
		memset(pgtable_virt, 0, 4096);
	} else if (flags & PAGE_USER) {
		/* User access requires permission at both paging levels. */
		page_dir[pde_i] |= PAGE_USER;
	}

	uint32_t* table = pte_get_table(pde_i);
	table[pte_i] = phys_addr | flags | PAGE_PRESENT;

	__asm__ __volatile__("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

void vmm_unmap(uint32_t virt_addr) {
	uint32_t pde_i = virt_addr >> 22;
	uint32_t pte_i = (virt_addr >> 12) & 0b1111111111;
	uint32_t* page_dir = current_page_dir();

	if (!(page_dir[pde_i] & PAGE_PRESENT)) {
		return;
	}

	uint32_t* table = pte_get_table(pde_i);
	table[pte_i] = 0;

	__asm__ __volatile__("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

void vmm_map_in(void* addr_space, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
	uint32_t target_cr3 = (uint32_t)addr_space;
	if (!target_cr3) {
		return;
	}

	uint32_t saved_flags = irq_save_disable();
	uint32_t saved_cr3 = read_cr3();

	if (target_cr3 != saved_cr3) {
		write_cr3(target_cr3);
	}
	vmm_map(virt_addr, phys_addr, flags);
	if (target_cr3 != saved_cr3) {
		write_cr3(saved_cr3);
	}

	irq_restore(saved_flags);
}

void vmm_map_in_run(void* addr_space, uint32_t virt_addr, const uint32_t* phys, uint32_t count, uint32_t flags) {
	uint32_t target_cr3 = (uint32_t)addr_space;
	if (!target_cr3 || !phys || count == 0) {
		return;
	}

	uint32_t saved_flags = irq_save_disable();
	uint32_t saved_cr3 = read_cr3();

	if (target_cr3 != saved_cr3) {
		write_cr3(target_cr3);
	}

	for (uint32_t i = 0; i < count; i++) {
		vmm_map(virt_addr + i * PAGE_SIZE, phys[i], flags);
	}

	if (target_cr3 != saved_cr3) {
		write_cr3(saved_cr3);
	}

	irq_restore(saved_flags);
}

void vmm_unmap_in(void* addr_space, uint32_t virt_addr) {
	uint32_t target_cr3 = (uint32_t)addr_space;
	if (!target_cr3) {
		return;
	}

	uint32_t saved_flags = irq_save_disable();
	uint32_t saved_cr3 = read_cr3();

	if (target_cr3 != saved_cr3) {
		write_cr3(target_cr3);
	}
	vmm_unmap(virt_addr);
	if (target_cr3 != saved_cr3) {
		write_cr3(saved_cr3);
	}

	irq_restore(saved_flags);
}

uint32_t vmm_get_phys(uint32_t virt_addr) {
	uint32_t pde_i = virt_addr >> 22;
	uint32_t pte_i = (virt_addr >> 12) & 0b1111111111;
	uint32_t* page_dir = current_page_dir();

	if (!(page_dir[pde_i] & PAGE_PRESENT)) {
		return 0;
	}

	uint32_t* table = pte_get_table(pde_i);
	if (!(table[pte_i] & PAGE_PRESENT)) {
		return 0;
	}

	return table[pte_i] & ~0xFFF;
}

void* vmm_get_kernel_page_dir(void) {
	return (void*)((uint32_t)boot_page_dir - KERNEL_VIRT_OFFSET);
}

void vmm_destroy_page_dir(void* addr_space) {
	uint32_t dir_phys = (uint32_t)addr_space;
	if (!dir_phys || dir_phys == (uint32_t)vmm_get_kernel_page_dir()) {
		return; // never free the kernel address space
	}

	uint32_t saved_flags = irq_save_disable();
	uint32_t saved_cr3 = read_cr3();
	if (dir_phys != saved_cr3) {
		write_cr3(dir_phys);
	}

	uint32_t* page_dir = current_page_dir();
	for (uint32_t i = 0; i < KERNEL_PDE_START; i++) {
		if (!(page_dir[i] & PAGE_PRESENT)) {
			continue;
		}

		uint32_t* table = pte_get_table(i);
		for (uint32_t j = 0; j < 1024; j++) {
			if (table[j] & PAGE_PRESENT) {
				pmm_freep(table[j] & ~0xFFF);
			}
		}

		pmm_freep(page_dir[i] & ~0xFFF);
		page_dir[i] = 0;
	}

	if (dir_phys != saved_cr3) {
		write_cr3(saved_cr3);
	}
	irq_restore(saved_flags);

	pmm_freep(dir_phys);
}

void* vmm_create_page_dir(void) {
	uint32_t page_dir_phys = pmm_allocp();
	klog(KLOG_INFO, "new page dir phys = 0x%x\n", page_dir_phys);
	if (page_dir_phys == 0) {
		return nullptr;
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
	uint32_t* active_page_dir = current_page_dir();
	for (uint32_t i = KERNEL_PDE_START; i < RECURSIVE_PDE_INDEX; i++) {
		page_dir[i] = active_page_dir[i];
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

/* Zero one physical frame out of band, then leave it unmapped.
 *
 * The frame has to be cleared through a scratch mapping rather than through its
 * eventual home: a PROT_NONE page is installed read-only even for the kernel,
 * since the x86 R/W bit gates supervisor access too, so there would be no way to
 * zero it in place after mapping. PAGE_DIR_TEMP_MAP is the same scratch page
 * vmm_create_page_dir() uses -- it sits above the vmalloc heap, so nothing else
 * can be living there. */
static void vmm_zero_frame(uint32_t phys) {
	vmm_map(PAGE_DIR_TEMP_MAP, phys, PAGE_RW);
	/* PAGE_DIR_TEMP_MAP's PDE (770) is not pre-created by vmm_init(), so the
	 * map above may itself have had to lazily allocate a page table. If that
	 * failed under memory pressure, memset would fault on an unmapped address
	 * and panic the kernel, so confirm the scratch mapping before writing. A
	 * frame left unzeroed here is never installed, because the map at its real
	 * home fails for the same reason and the run is rolled back. */
	if (vmm_get_phys(PAGE_DIR_TEMP_MAP) == phys) {
		memset((void*)PAGE_DIR_TEMP_MAP, 0, PAGE_SIZE);
	}
	vmm_unmap(PAGE_DIR_TEMP_MAP);
}

int vmm_map_zeroed_run(uint32_t virt, const uint32_t* phys, uint32_t count, uint32_t flags) {
	if (count == 0) {
		return 0;
	}
	if (!phys) {
		return -1;
	}

	/* pmm_allocp() is an unsynchronized read-modify-write on a shared bitmap and
	 * the kernel is preemptive, and the lazily allocated page tables inside
	 * vmm_map() come from the same allocator, so the whole run goes under one
	 * irq mask. This also collapses the per-page invlpg into a single batch. */
	uint32_t saved_flags = irq_save_disable();

	for (uint32_t i = 0; i < count; i++) {
		uint32_t v = virt + i * PAGE_SIZE;
		vmm_zero_frame(phys[i]);
		vmm_map(v, phys[i], flags);

		/* vmm_map() reports failure by doing nothing when it cannot allocate a
		 * page table, so confirm the install actually landed. Checking this is
		 * the difference between a caller that gets NOMEM and a caller that gets
		 * an address that faults on first touch. */
		if (vmm_get_phys(v) != phys[i]) {
			for (uint32_t j = 0; j < count; j++) {
				vmm_unmap(virt + j * PAGE_SIZE);
			}
			irq_restore(saved_flags);
			return -1;
		}
	}

	irq_restore(saved_flags);
	return 0;
}

/* Walk [begin, end) one page at a time looking for the first run of `count`
 * unmapped pages, returning its start or 0. Single linear pass with a run-length
 * counter, so a run already in progress at `begin` is picked up rather than
 * requiring the run to start exactly there. A PDE that is absent is treated as
 * 1024 free pages without touching a page table. */
static uint32_t vmm_scan_free(uint32_t begin, uint32_t end, uint32_t count) {
	if (begin >= end || count == 0) {
		return 0;
	}

	uint32_t start_pde = begin >> 22;
	uint32_t start_pte = (begin >> 12) & 0x3FF;
	uint32_t end_pde = end >> 22;
	uint32_t end_pte = (end >> 12) & 0x3FF;
	uint32_t run_start = 0;
	uint32_t run_len = 0;

	for (uint32_t pde_i = start_pde; pde_i <= end_pde && pde_i < KERNEL_PDE_START; pde_i++) {
		uint32_t* page_dir = current_page_dir();
		uint32_t* table = (page_dir[pde_i] & PAGE_PRESENT) ? pte_get_table(pde_i) : NULL;
		uint32_t pte_lo = (pde_i == start_pde) ? start_pte : 0;
		uint32_t pte_hi = (pde_i == end_pde) ? end_pte : PAGE_DIR_ENTRIES;

		for (uint32_t pte_i = pte_lo; pte_i < pte_hi; pte_i++) {
			if (table && (table[pte_i] & PAGE_PRESENT)) {
				run_len = 0;
				continue;
			}

			if (run_len == 0) {
				run_start = (pde_i << 22) | (pte_i << 12);
			}
			if (++run_len == count) {
				return run_start;
			}
		}
	}

	return 0;
}

uint32_t vmm_find_free_run(uint32_t hint, uint32_t count) {
	if (count == 0) {
		return 0;
	}

	/* begin always lands inside user space: an absent, out-of-range, or
	 * misaligned hint degrades to "search the whole range" rather than to a
	 * bogus start address. Never scan below USER_SPACE_BASE -- those pages
	 * either belong to nothing or are not valid user pointers. */
	uint32_t begin = USER_SPACE_BASE;
	if (hint != 0 && hint >= USER_SPACE_BASE && hint < USER_SPACE_END) {
		begin = hint & ~(uint32_t)(PAGE_SIZE - 1);
	}

	uint32_t run = vmm_scan_free(begin, USER_SPACE_END, count);
	if (run != 0 || begin == USER_SPACE_BASE) {
		return run;
	}

	/* Nothing above the hint -- look below it, so a bad hint costs a scan
	 * rather than a failed request. */
	return vmm_scan_free(USER_SPACE_BASE, begin, count);
}

int vmm_unmap_run_free(uint32_t virt, uint32_t count) {
	uint32_t saved_flags = irq_save_disable();

	for (uint32_t i = 0; i < count; i++) {
		uint32_t v = virt + i * PAGE_SIZE;
		/* vmm_unmap() does not free the frame, and a page that is already
		 * unmapped reports 0 here, which is what makes gaps in the range
		 * harmless. pmm_freep() would corrupt the bitmap given frame 0. */
		uint32_t frame = vmm_get_phys(v);
		if (frame != 0) {
			vmm_unmap(v);
			pmm_freep(frame);
		}
	}

	irq_restore(saved_flags);
	return 0;
}
