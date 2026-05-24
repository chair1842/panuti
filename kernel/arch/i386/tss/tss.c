#include "tss.h"
#include "../gdt/gdt.h"

#define KERNEL_DATA_SEG 0x10

static tss_t tss;

extern uint32_t stack_top;

void tss_init(void) {
	uint32_t base = (uint32_t)&tss;
	uint32_t limit = sizeof(tss_t) - 1;

	// install TSS descriptor in GDT at index 5
	gdt_set_entry(5, base, limit, 0x89, 0x00);

	// fill TSS
	tss.ss0 = KERNEL_DATA_SEG;
	tss.esp0 = (uint32_t)&stack_top;
	tss.iomap_base = sizeof(tss_t);

	// load task register with TSS selector (index 5, GDT, RPL 0) = 0x28
	__asm__ __volatile__("ltr %%ax" :: "a"(0x28));
}

void tss_set_kernel_stack(uint32_t esp0) {
	tss.esp0 = esp0;
}