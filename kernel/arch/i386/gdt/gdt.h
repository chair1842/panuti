#ifndef ARCH_I386_GDT_H
#define ARCH_I386_GDT_H

#include <stdint.h>

typedef struct gdtEntry gdtEntry_t;
struct gdtEntry {
	uint16_t low_limit = 0;
	uint16_t low_base = 0;
	uint8_t mid_base = 0;
	uint8_t access = 0;
	uint8_t hi_limit_n_flags = 0; // 4 bits for high limit, 4 bits for flags
	uint8_t hi_base = 0;
} __attribute__((packed));

typedef struct gdtr gdtr_t;
struct gdtr {
	uint16_t limit = 0;
	uint32_t base = 0;
} __attribute__((packed));

void gdt_init();
void gdt_load(gdtr_t* gdtr);
void gdt_reload_segments();
void gdt_set_entry(uint8_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags);

#endif