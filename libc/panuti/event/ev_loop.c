/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/errno.h>
#include <panuti/handle.h>

#include "ev_internal.h"

int ev_loop(int32_t timeout_cs) {
	if (ev_count == 0) {
		return PANUTIERRNO_INVALIDARG;
	}

	int fds[EV_MAX_FDS];
	for (uint32_t i = 0; i < ev_count; i++) {
		fds[i] = ev_entries[i].fd;
	}

	uint32_t which = 0;
	int rc = hwait(fds, ev_count, timeout_cs, &which);
	if (rc != 0) {
		return rc;
	}

	if (which >= ev_count) {
		return PANUTIERRNO_PLAINERR;
	}

	int fd = ev_entries[which].fd;
	ev_callback_t cb = ev_entries[which].cb;
	void* arg = ev_entries[which].arg;
	cb(fd, arg);
	return 0;
}
