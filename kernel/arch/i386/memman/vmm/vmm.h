/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef ARCH_I386_MEMMAN_VMM_H
#define ARCH_I386_MEMMAN_VMM_H

#include <stdint.h>

void vmm_init(void);
void vmm_map(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
void vmm_unmap(uint32_t virt_addr);
void vmm_map_in(void* addr_space, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);

// map `count` consecutive virtual pages starting at `virt_addr` onto the
// frames listed in phys[0..count), switching cr3 at most once. every cr3
// write flushes the tlb, so mapping a run through vmm_map_in pays two
// flushes per page.
void vmm_map_in_run(void* addr_space, uint32_t virt_addr, const uint32_t* phys, uint32_t count, uint32_t flags);
void vmm_unmap_in(void* addr_space, uint32_t virt_addr);
uint32_t vmm_get_phys(uint32_t virt_addr);
/* Returns an opaque address-space handle containing the page directory's CR3 address. */
void* vmm_create_page_dir(void);
/* Frees every user frame, page table, and the page directory of a created address space. */
void vmm_destroy_page_dir(void* addr_space);
void* vmm_get_kernel_page_dir(void);

/*
 * Mapping primitives backing the userspace anonymous page allocator
 * (mmapan/munmap). All of these operate on the address space currently loaded
 * in CR3, which for a syscall handler is the calling task's.
 */

/* Map `count` consecutive pages at `virt` onto phys[0..count), zeroing every
 * frame before it becomes reachable at `virt`. A reused frame can hold the
 * previous tenant's data, so a process must never be able to observe a page
 * between install and zero.
 *
 * Returns 0 on success. On failure (a page table could not be allocated for one
 * of the pages) it unmaps the whole run again so no partial mapping survives,
 * and returns nonzero; the frames in phys[] are the caller's to free either
 * way. */
int vmm_map_zeroed_run(uint32_t virt, const uint32_t* phys, uint32_t count, uint32_t flags);

/* Find `count` consecutive unmapped pages in the current address space,
 * returning the first free virtual address, or 0 if no such run exists.
 *
 * `hint` is a preference, not a contract: it is rounded down to a page and
 * ignored if it is outside user space. If the hinted region holds no run, the
 * search wraps and retries from the base of user space, so a stale or bogus
 * hint costs a scan but never fails a request that could have succeeded.
 *
 * The walk descends into page tables rather than trusting a single present PDE,
 * because vmm_unmap clears a PTE but leaves an emptied PDE in place -- a
 * present-PDE-holding-an-all-zero-table is a 4 MiB hole, not an occupied range. */
uint32_t vmm_find_free_run(uint32_t hint, uint32_t count);

/* Unmap `count` consecutive pages at `virt` and return each frame it freed to
 * the physical allocator. Pages that are already unmapped are skipped, so the
 * range is allowed to contain gaps. Returns 0. */
int vmm_unmap_run_free(uint32_t virt, uint32_t count);

#endif
