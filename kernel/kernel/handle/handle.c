#include "kernel/handle/inode_type.h"
#include <kernel/handle/handle.h>
#include <panuti/errno.h>
#include <kernel/sched/task.h>

int op_not_supported_rw(void* impl, void* buf, size_t len) {
	(void)impl; (void)buf; (void)len;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

int op_not_supported_w(void* impl, const void* buf, size_t len) {
	(void)impl; (void)buf; (void)len;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

int op_not_supported_act(void* impl) {
	(void)impl;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

int op_not_supported_rdy(void* impl) {
	(void)impl;
	return -1;
}

int op_not_supported_close(void* impl, struct task* self) {
	(void)impl; (void)self;
	return 0;
}

int handle_alloc(task_t* t, inode_type_t type) {
	for (int i = 0; i < MAX_HANDLES; i++) {
		if (t->handles[i].type == INODE_NONE) {
			t->handles[i].type = type;
			return i;
		}
	}
	
	return PANUTIERRNO_PLAINERR;
}

void handle_free(task_t* t, int fd) {
	if (fd < 0 || fd >= MAX_HANDLES) {
		return;
	}
	
	t->handles[fd].type = INODE_NONE;
	t->handles[fd].impl = NULL;
	t->handles[fd].ops = NULL;
	t->handles[fd].inode = NULL;
}