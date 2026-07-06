#include "idt.h"
#include <stdint.h>

#define IDT_FLAGS 0x8E
#define IDT_SELECTOR 0x08

typedef struct idtEntry idtEntry_t;
struct idtEntry {
	uint16_t base_low;    // lower 16 bits of handler address
	uint16_t selector;    // kernel code segment selector
	uint8_t  zero;        // always 0
	uint8_t  flags;       // type and attributes
	uint16_t base_high;   // upper 16 bits of handler address
} __attribute__((packed));

typedef struct idtr idtr_t;
struct idtr {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));


extern void idt_load(idtr_t* idtr);


extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);


static idtEntry_t idt[256];
static idtr_t idtr;

static void idt_set_entry(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags);

static void idt_set_entry(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags) {
	idt[vector].base_low = handler & 0xFFFF;
	idt[vector].selector = selector;
	idt[vector].zero = 0;
	idt[vector].flags = flags;
	idt[vector].base_high = (handler >> 16) & 0xFFFF;
}

void idt_init(void) {
	idtr.limit = sizeof(idt) - 1;
	idtr.base = (uint32_t)idt;

	// set from isr0 to isr31
	idt_set_entry(0, (uint32_t)isr0, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(1, (uint32_t)isr1, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(2, (uint32_t)isr2, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(3, (uint32_t)isr3, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(4, (uint32_t)isr4, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(5, (uint32_t)isr5, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(6, (uint32_t)isr6, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(7, (uint32_t)isr7, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(8, (uint32_t)isr8, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(9, (uint32_t)isr9, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(10, (uint32_t)isr10, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(11, (uint32_t)isr11, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(12, (uint32_t)isr12, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(13, (uint32_t)isr13, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(14, (uint32_t)isr14, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(15, (uint32_t)isr15, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(16, (uint32_t)isr16, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(17, (uint32_t)isr17, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(18, (uint32_t)isr18, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(19, (uint32_t)isr19, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(20, (uint32_t)isr20, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(21, (uint32_t)isr21, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(22, (uint32_t)isr22, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(23, (uint32_t)isr23, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(24, (uint32_t)isr24, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(25, (uint32_t)isr25, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(26, (uint32_t)isr26, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(27, (uint32_t)isr27, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(28, (uint32_t)isr28, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(29, (uint32_t)isr29, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(30, (uint32_t)isr30, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(31, (uint32_t)isr31, IDT_SELECTOR, IDT_FLAGS);

	// set from irq0 to irq15
	idt_set_entry(32, (uint32_t)irq0, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(33, (uint32_t)irq1, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(34, (uint32_t)irq2, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(35, (uint32_t)irq3, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(36, (uint32_t)irq4, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(37, (uint32_t)irq5, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(38, (uint32_t)irq6, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(39, (uint32_t)irq7, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(40, (uint32_t)irq8, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(41, (uint32_t)irq9, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(42, (uint32_t)irq10, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(43, (uint32_t)irq11, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(44, (uint32_t)irq12, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(45, (uint32_t)irq13, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(46, (uint32_t)irq14, IDT_SELECTOR, IDT_FLAGS);
	idt_set_entry(47, (uint32_t)irq15, IDT_SELECTOR, IDT_FLAGS);

	idt_load(&idtr);
}