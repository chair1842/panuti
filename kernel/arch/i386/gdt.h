#ifndef ARCH_I386_GDT_H
#define ARCH_I386_GDT_H

#include <stdint.h>

struct gdt_entry {
	uint16_t low_limit;
	uint16_t low_base;
	uint8_t mid_base;
	uint8_t access;
	uint8_t hi_limit_n_flags; // 4 bits for high limit, 4 bits for flags
	uint8_t hi_base;
} __attribute__((packed));

struct gdtr {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));

void gdt_init();
void gdt_load(struct gdtr* gdtr);
void gdt_reload_segments();

#endif