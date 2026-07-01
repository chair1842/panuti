#include <kernel/timer.h>
#include "intpt/handlers/main.h"
#include "intpt/handlers/handlers.h"

#define TIMER_FREQ 100

void kearly(void) {
	register_handler(0, dvbz_handler);
	register_handler(6, invalid_opcode_handler);
	register_handler(8, double_fault_handler);
	register_handler(13, gpf_handler);
	register_handler(14, page_fault_handler);

	timer_init(TIMER_FREQ);
}