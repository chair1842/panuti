#ifndef ARCH_I386_TSS_H
#define ARCH_I386_TSS_H

#include <stdint.h>

typedef struct tss tss_t;
struct tss {
	uint32_t prev_tss = 0;
	uint32_t esp0 = 0;
	uint32_t ss0 = 0;
	uint32_t esp1 = 0, ss1 = 0;
	uint32_t esp2 = 0, ss2 = 0;
	uint32_t cr3 = 0;
	uint32_t eip = 0, eflags = 0;
	uint32_t eax = 0, ecx = 0, edx = 0, ebx = 0;
	uint32_t esp = 0, ebp = 0, esi = 0, edi = 0;
	uint32_t es = 0, cs = 0, ss = 0, ds = 0, fs = 0, gs = 0;
	uint32_t ldt = 0;
	uint16_t trap = 0, iomap_base = 0;
} __attribute__((packed));

void tss_init(void);
void tss_set_kernel_stack(uint32_t esp0);

#endif