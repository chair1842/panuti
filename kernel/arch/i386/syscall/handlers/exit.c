#include "handlers.h"
#include <panuti/errno.h>
#include "kernel/sched/sched.h"
#include "kernel/sched/task.h"
#include <stdint.h>

int32_t syshandler_exit(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	uint32_t exit_code = a1;
	(void)exit_code;

	task_t* current = sched_current();
	current->state = TASK_TERMINATED;
	sched_schedule();

	return PANUTIERRNO_PLAINSUCCESS;
}