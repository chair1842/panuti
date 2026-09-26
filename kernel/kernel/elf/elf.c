/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/elf.h>
#include <kernel/memman/memman.h>

#define PAGE_SIZE 0x1000
#define MAX_SEG_PAGES 256

/* Dedicated scratch address for ELF segment loading.
 * Must not collide with TEMP_MAP_BASE (0xC0820000) used by map_physical_temp,
 * nor the vmalloc heap (capped at 0xC0800000). */
#define ELF_LOAD_SCRATCH 0xC0A00000

int elf_load_segments(addr_space_t addr_space, const void* elf_data, const elf_loadable_segment_t* segs, int nsegs) {
	for (int s = 0; s < nsegs; s++) {
		const elf_loadable_segment_t* seg = &segs[s];

		uint32_t vaddr  = (uint32_t)seg->vaddr;
		uint32_t offset = (uint32_t)seg->offset;
		uint32_t filesz = (uint32_t)seg->filesz;

		uint32_t page_start  = vaddr & ~(PAGE_SIZE - 1);
		uint32_t in_page_off = vaddr - page_start;

		// compute the span in 64 bits: memsz near 2^32 would wrap the sum and
		// defeat the MAX_SEG_PAGES check below
		uint64_t seg_span = (uint64_t)in_page_off + (uint64_t)seg->memsz;
		if (seg_span > (uint64_t)MAX_SEG_PAGES * PAGE_SIZE) {
			return -1;
		}
		uint32_t num_pages = (uint32_t)((seg_span + PAGE_SIZE - 1) / PAGE_SIZE);

		if (num_pages > MAX_SEG_PAGES) {
			return -1;
		}

		uint32_t map_flags = MEMMAN_PRESENT | MEMMAN_USER | MEMMAN_RW;
		uint32_t phys_frames[MAX_SEG_PAGES];

		uint32_t mapped = 0;
		for (uint32_t p = 0; p < num_pages; p++) {
			uint32_t phys = memman_alloc_frame();
			if (!phys) {
				// nothing is mapped until the whole run is collected, so a
				// short allocation has to hand the frames back directly
				for (uint32_t q = 0; q < mapped; q++) {
					memman_free_frame(phys_frames[q]);
				}

				return -1;
			}

			phys_frames[p] = phys;
			mapped++;
		}

		// one cr3 switch for the whole segment rather than one per page
		memman_map_in_run(addr_space, page_start, phys_frames, num_pages, map_flags);

		for (uint32_t p = 0; p < num_pages; p++) {
			memman_map(ELF_LOAD_SCRATCH, phys_frames[p], MEMMAN_PRESENT | MEMMAN_RW);
			uint8_t* dst = (uint8_t*)ELF_LOAD_SCRATCH;
			uint32_t start = (p == 0) ? in_page_off : 0;

			for (uint32_t off = start; off < PAGE_SIZE; off++) {
				uint32_t seg_byte = (p * PAGE_SIZE + off) - in_page_off;
				if (seg_byte >= (uint32_t)seg->memsz) {
					break;
				}

				dst[off] = (seg_byte < filesz)
					? ((const uint8_t*)elf_data)[offset + seg_byte]
					: 0;
			}

			memman_unmap(ELF_LOAD_SCRATCH);
		}
	}

	return 0;
}