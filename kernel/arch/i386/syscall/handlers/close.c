#include <kernel/handle/handle.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>

int32_t syshandler_close(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	int desc = (int)a1;

	task_t* t = sched_current();
	if (desc < 0 || desc >= MAX_HANDLES || 	t->handles[desc].type == INODE_NONE) {
		return PANUTIERRNO_BADFD;
	}

	int ret = t->handles[desc].ops->close(t->handles[desc].impl, t);
	handle_free(t, desc);
	return ret;
}
