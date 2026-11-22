/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>
#include <kernel/memman/slab.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/handle/pipe.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include <string.h>

int32_t syshandler_mkpipe(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	size_t path_len = kernel_user_strlen(path);
	if (path_len == (size_t)-1 || !kernel_usrptr_verify(path, path_len + 1)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (path_len == 0) {
		return PANUTIERRNO_INVALIDARG;
	}

	size_t end = path_len;
	while (end > 1 && path[end - 1] == '/') {
		end--;
	}

	const char* leaf = path + end;
	while (leaf > path && leaf[-1] != '/') {
		leaf--;
	}

	size_t leaf_len = (size_t)(path + end - leaf);
	if (leaf_len == 0 || leaf_len >= REG_MAX_NAME_LEN) {
		return PANUTIERRNO_INVALIDARG;
	}

	size_t dir_len = (size_t)(leaf - path);
	char dirpart[REG_MAX_NAME_LEN];
	if (dir_len == 0) {
		dirpart[0] = '.';
		dirpart[1] = '\0';
	} else {
		if (dir_len >= sizeof(dirpart)) {
			return PANUTIERRNO_INVALIDARG;
		}

		memcpy(dirpart, path, dir_len);
		dirpart[dir_len] = '\0';
	}

	task_t* t = sched_current();
	if (!t) {
		return PANUTIERRNO_PLAINERR;
	}

	inode_t* dir = registry_resolve(t->cwd, dirpart);
	if (!dir || dir->type != INODE_DIR || dir->mnt) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	if (registry_finddirent(dir, leaf, leaf_len)) {
		return PANUTIERRNO_EXISTS;
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

	int rc = registry_link(n, dir, leaf, leaf_len);
	inode_unref(n);
	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		pipe_end_unref(read_end);
		return rc;
	}

	int des = handle_alloc(t, INODE_PIPE);
	if (des < 0) {
		registry_unlink(dir, leaf, leaf_len);
		pipe_end_unref(read_end);
		return PANUTIERRNO_NOFDS;
	}

	handle_t* h = task_return_hslot((uint32_t)des, t);
	if (!h) {
		handle_free(t, des);
		registry_unlink(dir, leaf, leaf_len);
		pipe_end_unref(read_end);
		return PANUTIERRNO_PLAINERR;
	}

	h->type = INODE_PIPE;
	h->impl = read_end;
	h->ops = &pipe_ops;
	h->inode = nullptr;

	return des;
}
