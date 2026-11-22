/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_HANDLE_POINT_H
#define _KERNEL_HANDLE_POINT_H

#include <stdint.h>
#include <stdbool.h>
#include <kernel/handle/handle.h>
#include <kernel/sched/wait.h>

typedef struct point {
	uint32_t generation;
	bool user_activatable;
	bool owned;
	wait_queue_t waiters;
} point_t;

typedef struct point_handle {
	point_t* pt;
	uint32_t seen;
} point_handle_t;

void point_init(point_t* pt, bool user_activatable, bool owned);
point_t* point_create(bool user_activatable);
void point_destroy(point_t* pt);
void point_fire(point_t* pt);

extern const handle_ops_t point_ops;

#endif
