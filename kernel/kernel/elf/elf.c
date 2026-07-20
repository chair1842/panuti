#include <kernel/elf.h>
#include <kernel/memman/tempmap.h>

#define PAGE_SIZE 0x1000
#define MAX_SEG_PAGES 256 // 1MB max segment size for now

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

		// allocate + map every page up front, saving each physical frame
		// so the copy step below doesn't need to look it up through CR3
		for (uint32_t p = 0; p < num_pages; p++) {
			uint32_t phys = memman_alloc_frame();
			if (!phys) {
				return -1;
			}
			
			phys_frames[p] = phys;
			memman_map_in(addr_space, page_start + p * PAGE_SIZE, phys, map_flags);
		}

		// copy filesz bytes in, zero-fill the rest, page by page
		for (uint32_t p = 0; p < num_pages; p++) {
			void* dst = map_physical_temp(phys_frames[p], PAGE_SIZE);
			uint32_t start = (p == 0) ? in_page_off : 0;

			for (uint32_t off = start; off < PAGE_SIZE; off++) {
				uint32_t seg_byte = (p * PAGE_SIZE + off) - in_page_off;
				if (seg_byte >= memsz) {
					break;
				}
				
				((uint8_t*)dst)[off] = (seg_byte < filesz)
					? ((const uint8_t*)elf_data)[offset + seg_byte]
					: 0;
			}

			unmap_physical_temp(dst, PAGE_SIZE);
		}
	}

	return 0;
}