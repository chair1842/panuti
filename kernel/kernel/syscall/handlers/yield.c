#include <kernel/syscall/handlers.h>
#include <kernel/sched/sched.h>

int32_t syshandler_yield(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a1; (void)a2; (void)a3; (void)a4;

	// step aside and let someone else hog the cpu for a while
	sched_schedule();

	return 0;
}