/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/sched/sched.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>
#include <panuti/errno.h>
#include <stdint.h>

int32_t syshandler_resize(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3; (void)a4;
	int fd = a1;
	uint64_t* user_size = (uint64_t*)a2;

	if (
		!kernel_is_user_ptr(user_size)
		||
		!kernel_is_user_ptr((uint8_t*)user_size + sizeof(uint64_t) - 1)
	) {
		return PANUTIERRNO_INVALIDADDR;
	}
	
	uint64_t new_size = *user_size;

	if (new_size > INT64_MAX) {
		return PANUTIERRNO_INVALIDARG;
	}

	task_t* t = sched_current();

	if (fd < 0 || fd >= MAX_HANDLES || 	t->handles[fd].type == INODE_NONE) {
		return PANUTIERRNO_BADFD;
	}

	if (!t->handles[fd].ops->resize) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}
	
	return t->handles[fd].ops->resize(t->handles[fd].impl, new_size);
}