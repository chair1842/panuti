/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <kernel/syscall/leaf.h>
#include <kernel/memman/slab.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/handle/pipe.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>

int32_t syshandler_mkpipe(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;

	syscall_leaf_t leaf;
	int rc = syscall_leaf_begin((const char*)a1, &leaf);
	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		return rc;
	}

	pipe_t* p = kmalloc(sizeof(pipe_t), alignof(pipe_t));
	if (!p) {
		return PANUTIERRNO_NOMEM;
	}

	pipe_init(p);

	pipe_end_t* read_end = pipe_end_create(p, false);
	pipe_end_t* write_end = pipe_end_create(p, true);
	if (!read_end || !write_end) {
		kfree(read_end);
		kfree(write_end);
		kfree(p);
		return PANUTIERRNO_NOMEM;
	}

	fifo_node_t* fifo = kmalloc(sizeof(fifo_node_t), alignof(fifo_node_t));
	if (!fifo) {
		pipe_end_unref(write_end);
		pipe_end_unref(read_end);
		return PANUTIERRNO_NOMEM;
	}

	fifo->write_end = write_end;
	p->reader_gone = false;
	p->cookie = fifo;

	inode_t* n = registry_inode_alloc(INODE_PIPE);
	if (!n) {
		pipe_end_unref(write_end);
		kfree(fifo);
		pipe_end_unref(read_end);
		return PANUTIERRNO_NOMEM;
	}

	n->impl = fifo;
	n->ops = &fifo_ops;

	rc = registry_link(n, leaf.dir, leaf.name, leaf.name_len);
	inode_unref(n);
	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		pipe_end_unref(read_end);
		return rc;
	}

	task_t* t = sched_current();
	if (!t) {
		registry_unlink(leaf.dir, leaf.name, leaf.name_len);
		pipe_end_unref(read_end);
		return PANUTIERRNO_PLAINERR;
	}

	int des = handle_alloc(t, INODE_PIPE);
	if (des < 0) {
		registry_unlink(leaf.dir, leaf.name, leaf.name_len);
		pipe_end_unref(read_end);
		return PANUTIERRNO_NOFDS;
	}

	handle_t* h = task_return_hslot((uint32_t)des, t);
	if (!h) {
		handle_free(t, des);
		registry_unlink(leaf.dir, leaf.name, leaf.name_len);
		pipe_end_unref(read_end);
		return PANUTIERRNO_PLAINERR;
	}

	h->type = INODE_PIPE;
	h->impl = read_end;
	h->ops = &pipe_ops;
	h->inode = nullptr;

	return des;
}
