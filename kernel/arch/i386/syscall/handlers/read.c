#include <kernel/handle/handle.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>

int32_t syshandler_read(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a4;
	int desc = (int)a1;
	void* buf = (void*)a2;
	size_t len = (size_t)a3;

	task_t* t = sched_current();
	if (desc < 0 || desc >= MAX_HANDLES || 	t->handles[desc].type == INODE_NONE) {
		return PANUTIERRNO_BADFD;
	}

	return t->handles[desc].ops->read(t->handles[desc].impl, buf, len);
}
