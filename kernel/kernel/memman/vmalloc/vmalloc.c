/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/memman/vmalloc.h>
#include <stdint.h>
#include <kernel/klog.h>
#include <kernel/memman/memman.h>
#include <kernel/irq.h>

#define PAGE_ALIGN_UP(addr) (((addr) + 0xFFF) & ~0xFFF)

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2

/*
 * The kernel heap grows upward from _kernel_end. It must never run into the
 * fixed scratch windows used for temporary mappings -- vmm_create_page_dir()
 * maps PAGE_DIR_TEMP_MAP, map_physical_temp() uses TEMP_MAP_BASE, and the
 * ELF loader uses ELF_LOAD_SCRATCH. Those live at 0xC0800000+, so the heap is
 * capped just below them.
 */
#define VMALLOC_END 0xC0800000

#define VM_FREE_MAX 256

typedef struct {
	uint32_t vaddr;
	uint32_t npages;
} vm_free_t;

extern uint32_t _kernel_end;
static uint32_t vmalloc_next;
static vm_free_t vmfree[VM_FREE_MAX];
static uint32_t vmfree_count;

void vmalloc_init(void) {
	vmalloc_next = PAGE_ALIGN_UP((uint32_t)&_kernel_end);
	if (vmalloc_next == 0) {
		vmalloc_next = 4096;
	}
	vmfree_count = 0;
}

// Inserts a freed virtual range into the free list, coalescing it with every
// entry it intersects or sits directly adjacent to. Entries are therefore
// always pairwise disjoint: freeing an address range that is already free is a
// no-op, which prevents duplicate/overlapping entries from ever letting
// vmfree_take() hand the same live pages out twice.
// Caller must hold the vmalloc lock (irqs disabled).
static void vmfree_insert(uint32_t vaddr, uint32_t npages) {
	if (npages == 0) {
		return;
	}
	uint32_t lo = vaddr;
	uint32_t hi = vaddr + npages * 4096;
	for (uint32_t i = 0; i < vmfree_count; ) {
		uint32_t e_start = vmfree[i].vaddr;
		uint32_t e_end = e_start + vmfree[i].npages * 4096;
		if (e_end >= lo && e_start <= hi) {
			if (e_start < lo) {
				lo = e_start;
			}
			if (e_end > hi) {
				hi = e_end;
			}
			vmfree[i] = vmfree[--vmfree_count];
			i = 0;
		} else {
			i++;
		}
	}
	if (vmfree_count >= VM_FREE_MAX) {
		klog(KLOG_WARN, "vmalloc: free list overflow, dropping range %p\n", (void*)lo);
		return;
	}
	vmfree[vmfree_count].vaddr = lo;
	vmfree[vmfree_count].npages = (hi - lo) / 4096;
	vmfree_count++;
}

// Best-fit allocation from the free list. Returns 0 and removes the range
// (splitting off any surplus) when a fit exists.
static uint32_t vmfree_take(uint32_t npages) {
	int best = -1;
	for (uint32_t i = 0; i < vmfree_count; i++) {
		if (vmfree[i].npages >= npages &&
		    (best < 0 || vmfree[i].npages < vmfree[best].npages)) {
			best = (int)i;
		}
	}
	if (best < 0) {
		return 0;
	}
	uint32_t vaddr = vmfree[best].vaddr;
	if (vmfree[best].npages == npages) {
		vmfree[best] = vmfree[--vmfree_count];
	} else {
		vmfree[best].vaddr += npages * 4096;
		vmfree[best].npages -= npages;
	}
	return vaddr;
}

void* vmalloc_pg(void) {
	return vmalloc_pages(1);
}

// Maps `npages` consecutive virtual pages (fresh physical frames each) in one
// atomic step so concurrent callers can never interleave and fragment the range.
// Freed ranges are recycled first so the heap doesn't fill up for good.
void* vmalloc_pages(uint32_t npages) {
	if (npages == 0 || npages > 65536) {
		return nullptr;
	}

	uint32_t flags = irq_save_disable();
	uint32_t base = vmfree_take(npages);

	if (base == 0) {
		if (vmalloc_next > VMALLOC_END || npages > (VMALLOC_END - vmalloc_next) / 4096) {
			klog(KLOG_WARN, "vmalloc: heap exhausted for %u pages\n", npages);
			irq_restore(flags);
			return nullptr;
		}
		base = vmalloc_next;
		vmalloc_next += npages * 4096;
	}

	for (uint32_t i = 0; i < npages; i++) {
		uint32_t phys = memman_alloc_frame();
		if (phys == 0) {
			// Release the successfully mapped prefix back to the pool, then
			// return the still unmapped tail so no virtual space is leaked.
			for (uint32_t j = 0; j < i; j++) {
				memman_unmap(base + j * 4096);
				memman_free_frame(memman_get_phys(base + j * 4096));
			}
			vmfree_insert(base, i);
			vmfree_insert(base + i * 4096, npages - i);
			irq_restore(flags);
			return nullptr;
		}
		memman_map(base + i * 4096, phys, PAGE_PRESENT | PAGE_RW);
	}

	irq_restore(flags);

	return (void*)base;
}

void vmalloc_free_pages(void* addr, uint32_t npages) {
	for (uint32_t i = 0; i < npages; i++) {
		vmalloc_free((void*)((uint32_t)addr + i * 4096));
	}
}

void vmalloc_free(void* addr) {
	uint32_t virt = (uint32_t)addr;
	if ((virt & 0xFFF) != 0) {
		return;
	}

	uint32_t flags = irq_save_disable();
	uint32_t phys = memman_get_phys(virt);
	if (phys == 0) {
		irq_restore(flags);
		return;
	}

	memman_unmap(virt);
	memman_free_frame(phys);
	vmfree_insert(virt, 1);
	irq_restore(flags);
}