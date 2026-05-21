#include "pic.h"
#include "../../io.h"

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1+1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2+1)

#define PIC_EOI 0x20

#define ICW1_ICW4 0x01		/* Indicates that ICW4 will be present */
#define ICW1_SINGLE 0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL 0x08		/* Level triggered (edge) mode */
#define ICW1_INIT 0x10		/* Initialization - required! */

#define ICW4_8086 0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO 0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE 0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C		/* Buffered mode/master */
#define ICW4_SFNM 0x10		/* Special fully nested (not) */

#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

void pic_remap(int offset1, int offset2);

void pic_init(void) {
    pic_remap(PIC1_OFFSET, PIC2_OFFSET);
}

void pic_send_eoi(uint8_t irq) {
	if(irq >= 8) {
		outb(PIC2_COMMAND,PIC_EOI);
	}
	
	outb(PIC1_COMMAND,PIC_EOI);
}

void pic_remap(int offset1, int offset2) {
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
	iowait();
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	iowait();
	outb(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
	iowait();
	outb(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
	iowait();
	outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	iowait();
	outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
	iowait();
	
	outb(PIC1_DATA, ICW4_8086);               // ICW4: have the PICs use 8086 mode (and not 8080 mode)
	iowait();
	outb(PIC2_DATA, ICW4_8086);
	iowait();

	// Unmask both PICs.
	outb(PIC1_DATA, 0);
	outb(PIC2_DATA, 0);
}