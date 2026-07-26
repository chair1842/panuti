#include <kernel/handle/inode_type.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/handle/point.h>
#include <kernel/memman/slab.h>
#include <stdalign.h>
#include <kernel/sched/sched.h>

int point_activate_op(void* impl) {
	point_t* point = (point_t*)impl;
	return point_activate(point);
}

int point_rdy_op(void* impl) {
	point_t* point = (point_t*)impl;
	return point->pending; 
}

static handle_ops_t point_ops = {
	.read = op_not_supported_rw,
	.write = op_not_supported_w,
	.activate = point_activate_op,
	.ready = point_rdy_op,
	.close = op_not_supported_close,
};

point_t* point_alloc(task_t* owner) {
	point_t* point = kmalloc(sizeof(point_t), alignof(point_t));
	if (!point || !owner) {
		return NULL;
	}
	
	point->owner = owner;
	point->pending = false;
	point->state = POSTATE_VALID;
	
	return point;
}

point_t* point_create(const char* path, task_t* owner) {
	point_t* point = point_alloc(owner);
	if (!point) {
		return NULL;
	}
	
	if (registry_add(path, INODE_POINT, point, &point_ops) != 0) {
		kfree(point);
		return NULL;
	}
	
	return point;
}

int point_activate(point_t* point) {
	if (!point || point->state != POSTATE_VALID) {
		return -1;
	}

	point->pending = true;

	if (point->owner &&
		point->owner->state == TASK_BLOCKED)
	{
		task_wake(point->owner);
	}

	return 0;
}
