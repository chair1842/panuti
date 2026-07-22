#include <kernel/handle/point.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>

#define MAX_POINTS 16
static point_t points[MAX_POINTS];
static int point_count = 0;

static int point_wait(void* impl, struct task* self) {
	point_t* p = (point_t*)impl;

	if (p->waiter_count >= MAX_POINT_WAITERS) {
		return PANUTIERRNO_TOOMANYWAITERS;
	}

	p->waiters[p->waiter_count++] = self;
	sched_block(self);
	return PANUTIERRNO_PLAINSUCCESS;
}

static int point_activate(void* impl) {
	point_t* p = (point_t*)impl;

	for (int i = 0; i < p->waiter_count; i++) {
		sched_wake(p->waiters[i]);
	}
	
	p->waiter_count = 0;
	return PANUTIERRNO_PLAINSUCCESS;
}

static const handle_ops_t point_ops = {
	.read = op_not_supported_rw,
	.write = op_not_supported_w,
	.activate = point_activate,
	.wait = point_wait,
	.close = op_not_supported_close,
};

int point_create(const char* path) {
	if (point_count >= MAX_POINTS) {
		return -1;
	}

	point_t* p = &points[point_count];
	p->waiter_count = 0;

	if (registry_add(path, HANDLE_POINT, p, &point_ops) != 0) {
		return -1; // name taken or registry full
	}

	point_count++;
	return 0;
}