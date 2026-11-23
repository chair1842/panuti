/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_EVENT_INTERNAL_H
#define _PANUTI_EVENT_INTERNAL_H

#include <panuti/event.h>

#define EV_MAX_FDS 32

typedef struct {
	int fd;
	ev_callback_t cb;
	void* arg;
} ev_entry_t;

extern ev_entry_t ev_entries[EV_MAX_FDS];
extern uint32_t ev_count;

#endif
