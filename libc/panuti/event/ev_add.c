/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/errno.h>

#include "ev_internal.h"

ev_entry_t ev_entries[EV_MAX_FDS];
uint32_t ev_count;

int ev_add(int fd, ev_callback_t cb, void* arg) {
	if (!cb) {
		return PANUTIERRNO_INVALIDARG;
	}

	for (uint32_t i = 0; i < ev_count; i++) {
		if (ev_entries[i].fd == fd) {
			ev_entries[i].cb = cb;
			ev_entries[i].arg = arg;
			return 0;
		}
	}

	if (ev_count >= EV_MAX_FDS) {
		return PANUTIERRNO_NOFDS;
	}

	ev_entries[ev_count].fd = fd;
	ev_entries[ev_count].cb = cb;
	ev_entries[ev_count].arg = arg;
	ev_count++;
	return 0;
}
