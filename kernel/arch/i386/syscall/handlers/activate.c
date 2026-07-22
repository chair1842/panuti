#include <stdint.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>

int32_t syshandler_activate(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	int des = (int)a1;

	task_t* t = sched_current();
	if (des < 0 || des >= MAX_HANDLES || t->handles[des].type == HANDLE_NONE) {
		return PANUTIERRNO_BADFD;
	}

	return t->handles[des].ops->activate(t->handles[des].impl);
}