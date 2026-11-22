/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/point.h>
#include <kernel/memman/slab.h>
#include <kernel/irq.h>
#include <panuti/errno.h>

void point_init(point_t* pt, bool user_activatable, bool owned) {
	pt->generation = 0;
	pt->user_activatable = user_activatable;
	pt->owned = owned;
	wait_queue_init(&pt->waiters);
}

point_t* point_create(bool user_activatable) {
	point_t* pt = kmalloc(sizeof(point_t), alignof(point_t));
	if (!pt) {
		return nullptr;
	}

	point_init(pt, user_activatable, true);
	return pt;
}

void point_destroy(point_t* pt) {
	if (pt && pt->owned) {
		kfree(pt);
	}
}

void point_fire(point_t* pt) {
	if (!pt) {
		return;
	}

	uint32_t flags = irq_save_disable();
	pt->generation++;
	wait_queue_wake_all(&pt->waiters);
	irq_restore(flags);
}

static int point_open(void* impl, void** out_impl) {
	point_t* pt = impl;
	if (!pt) {
		return -1;
	}

	point_handle_t* h = kmalloc(sizeof(point_handle_t), alignof(point_handle_t));
	if (!h) {
		return -1;
	}

	uint32_t flags = irq_save_disable();
	h->pt = pt;
	h->seen = pt->generation;
	irq_restore(flags);

	*out_impl = h;
	return 0;
}

static int point_close(void* impl, struct task* self) {
	(void)self;
	kfree(impl);
	return 0;
}

static int point_activate(void* impl) {
	point_handle_t* h = impl;
	if (!h || !h->pt || !h->pt->user_activatable) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	point_fire(h->pt);
	return PANUTIERRNO_PLAINSUCCESS;
}

static int point_ready(void* impl) {
	point_handle_t* h = impl;
	if (!h || !h->pt) {
		return -1;
	}

	uint32_t flags = irq_save_disable();
	int ready = h->seen != h->pt->generation;
	if (ready) {
		h->seen = h->pt->generation;
	}
	irq_restore(flags);

	return ready ? 1 : 0;
}

static wait_queue_t* point_waitq(void* impl) {
	point_handle_t* h = impl;
	if (!h || !h->pt) {
		return nullptr;
	}

	return &h->pt->waiters;
}

const handle_ops_t point_ops = {
	.open = point_open,
	.activate = point_activate,
	.ready = point_ready,
	.waitq = point_waitq,
	.close = point_close,
};
