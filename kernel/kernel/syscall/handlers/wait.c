#include <kernel/syscall/handlers.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>

int32_t syshandler_wait(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3; (void)a4;
	pid_t target = (pid_t)a1;
	int* user_exit_code = (int*)a2;

	if (!kernel_is_user_range(user_exit_code, sizeof(int))) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* caller = sched_current();

	if (target == caller->pid) {
		return PANUTIERRNO_PLAINERR;
	}

	while (1) {
		int exit_code;
		int rc = task_wait_pid(target, &exit_code);

		if (rc == -1) {
			return PANUTIERRNO_NOTFOUND;
		}
		if (rc == 0) {
			*user_exit_code = exit_code;
			return PANUTIERRNO_PLAINSUCCESS;
		}

		// rc == 1: still running, block and retry
		caller->pid_waiting_on = target;
		task_block(caller);
	}
}