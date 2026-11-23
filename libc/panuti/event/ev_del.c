/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/errno.h>

#include "ev_internal.h"

int ev_del(int fd) {
	for (uint32_t i = 0; i < ev_count; i++) {
		if (ev_entries[i].fd != fd) {
			continue;
		}

		for (uint32_t j = i + 1; j < ev_count; j++) {
			ev_entries[j - 1] = ev_entries[j];
		}

		ev_count--;
		return 0;
	}

	return PANUTIERRNO_NOTFOUND;
}
