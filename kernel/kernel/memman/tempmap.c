#include <kernel/memman/tempmap.h>
#include <kernel/memman/memman.h>

#define TEMP_MAP_BASE  0xC0600000
#define PAGE_SIZE      0x1000

void* map_physical_temp(uint32_t phys, size_t size) {
	uint32_t phys_page_start = phys & ~(PAGE_SIZE - 1);
	uint32_t offset_in_page  = phys - phys_page_start;
	uint32_t num_pages = (offset_in_page + size + PAGE_SIZE - 1) / PAGE_SIZE;

	for (uint32_t i = 0; i < num_pages; i++) {
		memman_map(TEMP_MAP_BASE + i * PAGE_SIZE,
		           phys_page_start + i * PAGE_SIZE,
		           MEMMAN_PRESENT | MEMMAN_RW);
	}

	/* return a pointer that accounts for phys not being page-aligned */
	return (void*)(TEMP_MAP_BASE + offset_in_page);
}

void unmap_physical_temp(void* virt, size_t size) {
	uint32_t virt_addr = (uint32_t)virt;
	uint32_t page_start = virt_addr & ~(PAGE_SIZE - 1);
	uint32_t offset_in_page = virt_addr - page_start;
	uint32_t num_pages = (offset_in_page + size + PAGE_SIZE - 1) / PAGE_SIZE;

	for (uint32_t i = 0; i < num_pages; i++) {
		memman_unmap(page_start + i * PAGE_SIZE);
	}
}