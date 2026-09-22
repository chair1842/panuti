#include <kernel/timer.h>
#include "intpt/handlers/main.h"
#include "intpt/handlers/handlers.h"
#include <kernel/klog.h>
#include <kernel/serial.h>
#include "drivers/kbd/ps2.h"

#define TIMER_FREQ 100

void kearly(void) {
	serial_init();
	register_handler(0, dvbz_handler);
	register_handler(6, invalid_opcode_handler);
	register_handler(8, double_fault_handler);
	register_handler(13, gpf_handler);
	register_handler(14, page_fault_handler);

	kbd_ps2_init();

	timer_init(TIMER_FREQ);
	klog(KLOG_INFO, "kearly: timer initialized\n");
	__asm__ __volatile__("sti");
}