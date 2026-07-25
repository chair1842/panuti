#include <kernel/elf.h>
#include <kernel/memman/memman.h>

#define PAGE_SIZE 0x1000
#define MAX_SEG_PAGES 256

/* Dedicated scratch address for ELF segment loading.
 * Must not collide with TEMP_MAP_BASE (0xC0600000) used by map_physical_temp,
 * since the ELF source data may already be mapped there via kernel_get_init_module. */
#define ELF_LOAD_SCRATCH 0xC0700000

int elf_load_segments(addr_space_t addr_space, const void* elf_data, const elf_loadable_segment_t* segs, int nsegs) {
	for (int s = 0; s < nsegs; s++) {
		const elf_loadable_segment_t* seg = &segs[s];

		uint32_t vaddr  = (uint32_t)seg->vaddr;
		uint32_t offset = (uint32_t)seg->offset;
		uint32_t filesz = (uint32_t)seg->filesz;
		uint32_t memsz  = (uint32_t)seg->memsz;

		uint32_t page_start  = vaddr & ~(PAGE_SIZE - 1);
		uint32_t in_page_off = vaddr - page_start;
		uint32_t num_pages   = (in_page_off + memsz + PAGE_SIZE - 1) / PAGE_SIZE;

		if (num_pages > MAX_SEG_PAGES) {
			return -1;
		}

		uint32_t map_flags = MEMMAN_PRESENT | MEMMAN_USER | MEMMAN_RW;
		uint32_t phys_frames[MAX_SEG_PAGES];

		for (uint32_t p = 0; p < num_pages; p++) {
			uint32_t phys = memman_alloc_frame();
			if (!phys) {
				return -1;
			}

			phys_frames[p] = phys;
			memman_map_in(addr_space, page_start + p * PAGE_SIZE, phys, map_flags);
		}

		for (uint32_t p = 0; p < num_pages; p++) {
			memman_map(ELF_LOAD_SCRATCH, phys_frames[p], MEMMAN_PRESENT | MEMMAN_RW);
			uint8_t* dst = (uint8_t*)ELF_LOAD_SCRATCH;
			uint32_t start = (p == 0) ? in_page_off : 0;

			for (uint32_t off = start; off < PAGE_SIZE; off++) {
				uint32_t seg_byte = (p * PAGE_SIZE + off) - in_page_off;
				if (seg_byte >= memsz) {
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