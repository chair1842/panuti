/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/sched/sched.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>
#include <panuti/errno.h>
#include <panuti/syscall/seek.h>
#include <stdint.h>

int32_t syshandler_seek(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a4;
	int fd = a1;
	int64_t* user_off = (int64_t*)a2;
	int whence = (int)a3;

	if (
		!kernel_is_user_ptr(user_off)
		||
		!kernel_is_user_ptr((uint8_t*)user_off + sizeof(int64_t) - 1)
	) {
		return PANUTIERRNO_INVALIDADDR;
	}

	int64_t offset = *user_off;

	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
		return PANUTIERRNO_INVALIDARG;
	}

	task_t* t = sched_current();

	if (fd < 0 || fd >= MAX_HANDLES || t->handles[fd].type == INODE_NONE) {
		return PANUTIERRNO_BADFD;
	}

	if (!t->handles[fd].ops->seek) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	return t->handles[fd].ops->seek(t->handles[fd].impl, whence, offset);
}