#ifndef _KERNEL_HANDLE_POINT_H
#define _KERNEL_HANDLE_POINT_H

#include <stdbool.h>
#include <kernel/sched/task.h>

typedef enum {
	POSTATE_VALID,
	POSTATE_INVALID,
} point_state_t;

typedef struct point {
	task_t* owner;
	bool pending;
	task_t* waiter;
	point_state_t state;
} point_t;

point_t* point_alloc(task_t* owner);
point_t* point_create(const char* path, task_t* owner);
int point_activate(point_t* point);
int point_wait(point_t* point, task_t* self);

#endif