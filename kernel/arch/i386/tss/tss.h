#ifndef ARCH_I386_TSS_H
#define ARCH_I386_TSS_H

#include <stdint.h>

typedef struct tss tss_t;
struct tss{
	uint32_t prev_tss;
	uint32_t esp0;
	uint32_t ss0;
	uint32_t esp1, ss1;
	uint32_t esp2, ss2;
	uint32_t cr3;
	uint32_t eip, eflags;
	uint32_t eax, ecx, edx, ebx;
	uint32_t esp, ebp, esi, edi;
	uint32_t es, cs, ss, ds, fs, gs;
	uint32_t ldt;
	uint16_t trap, iomap_base;
} __attribute__((packed));

void tss_init(void);
void tss_set_kernel_stack(uint32_t esp0);

#endif