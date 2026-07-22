#include "handlers.h"
#include <stdint.h>
#include <kernel/sched/sched.h>

int32_t syshandler_getpid(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a1; (void)a2; (void)a3; (void)a4;
	return (int32_t)sched_current()->pid;
}