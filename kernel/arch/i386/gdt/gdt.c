#define GDT_ENTRIES 6

#include "gdt.h"

static gdtEntry_t gdt[GDT_ENTRIES];

void gdt_entry_helper(gdtEntry_t *entry, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags);

void gdt_entry_helper(gdtEntry_t* entry, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
	entry->low_limit = limit & 0xFFFF;
	entry->low_base = base & 0xFFFF;
	entry->mid_base = (base >> 16) & 0xFF;
	entry->access = access;
	entry->hi_limit_n_flags = ((limit >> 16) & 0x0F) | (flags << 4);
	entry->hi_base = (base >> 24) & 0xFF;
}

void gdt_set_entry(uint8_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
	gdt_entry_helper(&gdt[index], base, limit, access, flags);
}

void gdt_init() {

	// this will set up the GDT entries with a flat memory model.
	gdt_entry_helper(&gdt[0], 0, 0, 0, 0); // Null descriptor
	gdt_entry_helper(&gdt[1], 0, 0xFFFFF, 0x9A, 0x0C); // Code segment
	gdt_entry_helper(&gdt[2], 0, 0xFFFFF, 0x92, 0x0C); // Data segment
	gdt_entry_helper(&gdt[3], 0, 0xFFFFF, 0xFA, 0x0C); // User mode code segment
	gdt_entry_helper(&gdt[4], 0, 0xFFFFF, 0xF2, 0x0C); // User mode data segment
	// no tss segment yet, return to this later.

	// and lo it is done
	// not yet (。_。)

	// pass the limit and base to the assembly function to load the GDT
	gdtr_t gdtr = {
		.limit = (sizeof(gdtEntry_t) * GDT_ENTRIES) - 1,
		.base = (uint32_t)gdt
	};
	gdt_load(&gdtr);

	// then done
	gdt_reload_segments();

	return;
}