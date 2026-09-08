#include <kernel/syscall/handlers.h>
#include <kernel/timer.h>

int32_t syshandler_timesb(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a1; (void)a2; (void)a3; (void)a4;
	
	return (int32_t)timer_get_ticks();
}