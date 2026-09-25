/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include <stdint.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>

int32_t syshandler_stream_write(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a4;
	int stream_num = (int)a1;
	const void* buf = (const void*)a2;
	size_t len = (size_t)a3;

	if (!kernel_is_user_range((void*)buf, len)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();

	if (stream_num < 0 || stream_num >= t->no_out_streams) {
		return PANUTIERRNO_BADFD;
	}

	handle_t* h = &t->out_streams[stream_num];
	if (!h->ops || !h->ops->write) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	return h->ops->write(h->impl, buf, len);
}