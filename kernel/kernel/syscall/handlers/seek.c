/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/sched/sched.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>
#include <panuti/errno.h>
#include <panuti/seek.h>
#include <stdint.h>

int32_t syshandler_seek(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a4;
	int fd = a1;
	int64_t* user_off = (int64_t*)a2;
	int whence = (int)a3;

	if (!kernel_usrptr_verify(user_off, sizeof(int64_t))) {
		return PANUTIERRNO_INVALIDADDR;
	}

	int64_t offset = *user_off;

	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
		return PANUTIERRNO_INVALIDARG;
	}

	task_t* t = sched_current();

	handle_t* h = task_return_hslot((uint32_t)fd, t);
	if (!h) {
		return PANUTIERRNO_BADFD;
	}

	if (!h->ops->seek) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	return h->ops->seek(h->impl, whence, offset);
}