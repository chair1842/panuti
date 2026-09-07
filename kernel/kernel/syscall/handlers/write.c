#include <kernel/handle/handle.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>

int32_t syshandler_write(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a4;
	int desc = (int)a1;
	const void* buf = (const void*)a2;	
	size_t len = (size_t)a3;

	if (!kernel_is_user_range(buf, len)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();
	if (desc < 0 || desc >= MAX_HANDLES || 	t->handles[desc].type == INODE_NONE) {
		return PANUTIERRNO_BADFD;
	}

	return t->handles[desc].ops->write(t->handles[desc].impl, buf, len);
}