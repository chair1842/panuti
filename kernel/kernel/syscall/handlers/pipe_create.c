#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>
#include <panuti/errno.h>
#include <kernel/sched/sched.h>
#include <kernel/handle/pipe.h>

int32_t syshandler_pipe_create(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3; (void)a4;
	int* user_out_read = (int*)a1;
	int* user_out_write = (int*)a2;

	if (!kernel_is_user_ptr(user_out_read) || !kernel_is_user_ptr(user_out_write)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();

	int r_des, w_des;
	int rc = pipe_create_pair(t, &r_des, &w_des);
	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		return rc;
	}

	*user_out_read = r_des;
	*user_out_write = w_des;
	return PANUTIERRNO_PLAINSUCCESS;
}