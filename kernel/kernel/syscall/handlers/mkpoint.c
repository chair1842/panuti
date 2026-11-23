/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <kernel/syscall/leaf.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/point.h>
#include <kernel/handle/registry.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>

int32_t syshandler_mkpoint(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;

	syscall_leaf_t leaf;
	int rc = syscall_leaf_begin((const char*)a1, &leaf);
	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		return rc;
	}

	point_t* pt = point_create(true);
	if (!pt) {
		return PANUTIERRNO_NOMEM;
	}

	inode_t* n = registry_inode_alloc(INODE_POINT);
	if (!n) {
		point_destroy(pt);
		return PANUTIERRNO_NOMEM;
	}

	n->impl = pt;
	n->ops = &point_ops;

	rc = registry_link(n, leaf.dir, leaf.name, leaf.name_len);
	inode_unref(n);
	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		point_destroy(pt);
		return rc;
	}

	void* open_impl = pt;
	if (point_ops.open(pt, &open_impl) < 0) {
		registry_unlink(leaf.dir, leaf.name, leaf.name_len);
		return PANUTIERRNO_PLAINERR;
	}

	task_t* t = sched_current();
	if (!t) {
		point_ops.close(open_impl, t);
		registry_unlink(leaf.dir, leaf.name, leaf.name_len);
		return PANUTIERRNO_PLAINERR;
	}

	int des = handle_alloc(t, INODE_POINT);
	if (des < 0) {
		point_ops.close(open_impl, t);
		registry_unlink(leaf.dir, leaf.name, leaf.name_len);
		return PANUTIERRNO_NOFDS;
	}

	handle_t* h = task_return_hslot((uint32_t)des, t);
	if (!h) {
		point_ops.close(open_impl, t);
		handle_free(t, des);
		registry_unlink(leaf.dir, leaf.name, leaf.name_len);
		return PANUTIERRNO_PLAINERR;
	}

	h->type = INODE_POINT;
	h->impl = open_impl;
	h->ops = &point_ops;
	h->inode = n;
	inode_ref(n);

	return des;
}
