#include "pmm.h"
#include <stdlib.h>

#define PMM_BITMAP_SIZE 131072
#define PMM_PAGE_SIZE 4096

#define LOWMEM_START 0x0
#define LOWMEM_END 0x100000

#define KERNEL_START 0x100000
extern uint32_t _kernel_end;
#define KERNEL_END ((uint32_t)&_kernel_end - 0xC0000000)

// this is a 128KiB bitmap that tracks whether pages are free or not.
// each bit tracks one 4KiB page. 0 means used, 1 means free.
// this bitmap handles 4GiB of memory, which i think is the max on i686.
static uint8_t pmm_bitmap[PMM_BITMAP_SIZE];

uint32_t init_module_phys_start = 0;
uint32_t init_module_phys_end = 0;

// set the bit for a page in the bitmap to 1, marking it free.
void pmm_clrp(uint32_t page) {
	// github copilot can do things i dont understand, or am willing to understand on my own.
	pmm_bitmap[page / 8] |= (1 << (page % 8));
}

// set the bit for a page in the bitmap to 0, marking it used.
void pmm_setp(uint32_t page) {
	pmm_bitmap[page / 8] &= ~(1 << (page % 8));
}

void pmm_init(multiboot_info_t* mbi) {
	uint8_t mbi_imp = (mbi->flags >> 6) & 1;
	if (!mbi_imp) {
		// No memory map provided by the bootloader. Abort and curse the bootloader with the souls of a thosand devils.
		abort();
		return;
	}

	// now we walk the mmap.
	multiboot_memory_map_t* entry = (multiboot_memory_map_t*) mbi->mmap_addr;
	while ((uint32_t)entry < mbi->mmap_addr + mbi->mmap_length) {
		if (entry->addr >= 0x100000000ULL) {
			// this region of memory is above 4GiB, which we dont support. skip it.
			entry = (multiboot_memory_map_t *)((uint32_t)entry + entry->size + sizeof(uint32_t));
			continue;
		}

		uint64_t region_end = entry->addr + entry->len;
		if (region_end > 0x100000000ULL)
			region_end = 0x100000000ULL;
			
		uint32_t start_page = (uint32_t)(entry->addr / PMM_PAGE_SIZE);
		uint32_t end_page = (uint32_t)(region_end / PMM_PAGE_SIZE);

		// hiiiiiiii i am a comment
		
		if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
			// this is a free region of memory. mark all the pages in it as free.
			for (uint32_t page = start_page; page < end_page; page++) {
				pmm_clrp(page);
			}
		}

		entry = (multiboot_memory_map_t *)((uint32_t)entry + entry->size + sizeof(uint32_t));
	}

	// now we mark low memory and kernel memory as used, since we know we will be using it.
	for (uint32_t page = LOWMEM_START / PMM_PAGE_SIZE; page < LOWMEM_END / PMM_PAGE_SIZE; page++) {
		pmm_setp(page);
	}
	for (uint32_t page = KERNEL_START / PMM_PAGE_SIZE; page < (KERNEL_END + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE; page++) {
		pmm_setp(page);
	}

	// reserve any multiboot modules (e.g. init.elf) so pmm_allocp() never hands out their frames.
	uint8_t mods_imp = (mbi->flags >> 3) & 1;
	if (mods_imp && mbi->mods_count > 0) {
		multiboot_module_t* mods = (multiboot_module_t*)mbi->mods_addr;
		for (uint32_t i = 0; i < mbi->mods_count; i++) {
			uint32_t start_page = mods[i].mod_start / PMM_PAGE_SIZE;
			uint32_t end_page = (mods[i].mod_end + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
			for (uint32_t page = start_page; page < end_page; page++) {
				pmm_setp(page);
			}
		}

		// stash the first module's location for the ELF loader to use later.
		init_module_phys_start = mods[0].mod_start;
		init_module_phys_end = mods[0].mod_end;
	}
}

uint32_t pmm_allocp(void) {
	for (uint32_t page = 0; page < PMM_BITMAP_SIZE * 8; page++) {
		if (pmm_bitmap[page / 8] & (1 << (page % 8))) {
			// this page is free. mark it as used and return its address.
			pmm_setp(page);
			return page * PMM_PAGE_SIZE;
		}
	}
	// no free pages found. return 0 to indicate failure.
	return 0;
}

void pmm_freep(uint32_t address) {
	// mark the page at the given address as free.
	uint32_t page = address / PMM_PAGE_SIZE;
	pmm_clrp(page);
}
