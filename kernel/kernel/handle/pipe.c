/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/pipe.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
#include <kernel/sched/sched.h>
#include <kernel/irq.h>
#include <string.h>
#include <panuti/errno.h>

static size_t pipe_read_raw(pipe_t* p, void* buf, size_t len) {
	if (len > p->count) {
		len = p->count;
	}

	uint8_t* out = buf;
	for (size_t i = 0; i < len; i++) {
		out[i] = p->buffer[p->head];
		p->head = (p->head + 1) % p->capacity;
	}

	p->count -= len;
	return len;
}

static size_t pipe_write_raw(pipe_t* p, const void* buf, size_t len) {
	size_t free_space = p->capacity - p->count;
	if (len > free_space) {
		len = free_space;
	}

	const uint8_t* in = buf;
	for (size_t i = 0; i < len; i++) {
		p->buffer[p->tail] = in[i];
		p->tail = (p->tail + 1) % p->capacity;
	}

	p->count += len;
	return len;
}

int pipe_read(void* impl, void* buf, size_t len) {
	pipe_end_t* end = impl;
	if (end->is_write_end) {
		return -1; // can't read from the write end
	}
	if (len == 0) {
		return 0;
	}

	pipe_t* p = end->pipe;

	while (1) {
		uint32_t flags = irq_save_disable();

		if (p->count > 0 || p->write_end_closed) {
			if (p->count == 0) {
				irq_restore(flags);
				return 0;
			}

			size_t n = pipe_read_raw(p, buf, len);
			wait_queue_wake_all(&p->writers_waiting);
			irq_restore(flags);
			return (int)n;
		}

		task_t* self = sched_current();
		if (!self || self->kill_pending) {
			irq_restore(flags);
			return -1;
		}

		wait_queue_add(&p->readers_waiting, self);
		task_block(self);
		wait_queue_remove(&p->readers_waiting, self);

		if (self->kill_pending) {
			irq_restore(flags);
			return -1;
		}

		irq_restore(flags);
	}
}

int pipe_write(void* impl, const void* buf, size_t len) {
	pipe_end_t* end = impl;
	if (!end->is_write_end) {
		return -1; // can't write to the read end
	}
	if (len == 0) {
		return 0;
	}

	pipe_t* p = end->pipe;
	if (p->read_end_closed) {
		return -1; // broken pipe
	}

	size_t atomic = p->capacity < PIPE_ATOMIC_WRITE ? p->capacity : PIPE_ATOMIC_WRITE;
	if (atomic == 0) {
		return -1;
	}

	size_t written = 0;

	while (written < len) {
		size_t chunk = len - written;
		if (chunk > atomic) {
			chunk = atomic;
		}

		uint32_t flags = irq_save_disable();

		if (p->read_end_closed) {
			irq_restore(flags);
			return written > 0 ? (int)written : -1;
		}

		if (p->capacity - p->count >= chunk) {
			size_t n = pipe_write_raw(p, (const uint8_t*)buf + written, chunk);
			written += n;
			wait_queue_wake_all(&p->readers_waiting);
			irq_restore(flags);
			continue;
		}

		task_t* self = sched_current();
		if (!self || self->kill_pending) {
			irq_restore(flags);
			return written > 0 ? (int)written : -1;
		}

		wait_queue_add(&p->writers_waiting, self);
		task_block(self);
		wait_queue_remove(&p->writers_waiting, self);

		if (self->kill_pending) {
			irq_restore(flags);
			return written > 0 ? (int)written : -1;
		}

		irq_restore(flags);
	}

	return (int)written;
}

int pipe_end_ref(pipe_end_t* end) {
	end->refcount++;
	return 0;
}

// the last holder of this end lets go: signal EOF to the peer end, then drop
// the end and the pipe reference it was holding
static void pipe_end_release(pipe_end_t* end) {
	pipe_t* p = end->pipe;
	uint32_t flags = irq_save_disable();

	if (end->is_write_end) {
		p->write_end_closed = true;
		wait_queue_wake_all(&p->readers_waiting);
	} else {
		p->read_end_closed = true;
		wait_queue_wake_all(&p->writers_waiting);
	}

	p->refcount--;
	bool free_pipe = p->refcount == 0;
	irq_restore(flags);

	if (free_pipe) {
		kfree(p);
	}

	kfree(end);
}

// one holder gives up its reference. an end is shared between the creating
// task's handle table and every child it was installed into, so it must only
// be released (and only signal EOF) once all of them have let go.
int pipe_end_unref(pipe_end_t* end) {
	if (--end->refcount > 0) {
		return 0;
	}

	pipe_end_release(end);
	return 0;
}

int pipe_close(void* impl, struct task* self) {
	(void)self;
	return pipe_end_unref((pipe_end_t*)impl);
}

pipe_end_t* pipe_end_create(pipe_t* pipe, bool is_write_end) {
	pipe_end_t* end = kmalloc(sizeof(pipe_end_t), alignof(pipe_end_t));
	if (!end) {
		return nullptr;
	}
	
	end->pipe = pipe;
	end->is_write_end = is_write_end;
	end->refcount = 1;
	return end;
}

static int pipe_activate(void* impl) {
	(void)impl;
	return -1;

	// pipes aren't points
	// what are points you ask? ooh, you'll soon find out
}

static int pipe_ready(void* impl) {
	(void)impl;
	return -1;
}

const handle_ops_t pipe_ops = {
	.read = pipe_read,
	.write = pipe_write,
	.activate = pipe_activate,
	.ready = pipe_ready,
	.close = pipe_close,
};

int pipe_create_pair(task_t* t, int* out_read, int* out_write) {
	pipe_t* p = kmalloc(sizeof(pipe_t), alignof(pipe_t));
	if (!p) {
		return PANUTIERRNO_PLAINERR;
	}

	p->capacity = PIPE_BUFFER_SIZE;
	p->head = 0;
	p->tail = 0;
	p->count = 0;
	p->refcount = 2;
	wait_queue_init(&p->readers_waiting);
	wait_queue_init(&p->writers_waiting);
	p->write_end_closed = false;
	p->read_end_closed = false;

	pipe_end_t* read_end = pipe_end_create(p, false);
	pipe_end_t* write_end = pipe_end_create(p, true);
	if (!read_end || !write_end) {
		kfree(read_end);
		kfree(write_end);
		kfree(p);
		return PANUTIERRNO_PLAINERR;
	}

	int r_des = handle_alloc(t, INODE_PIPE);
	if (r_des < 0) {
		kfree(read_end);
		kfree(write_end);
		kfree(p);
		return PANUTIERRNO_NOFDS;
	}

	int w_des = handle_alloc(t, INODE_PIPE);
	if (w_des < 0) {
		handle_free(t, r_des);
		kfree(read_end);
		kfree(write_end);
		kfree(p);
		return PANUTIERRNO_NOFDS;
	}

	handle_t* read_slot = task_return_hslot((uint32_t)r_des, t);
	handle_t* write_slot = task_return_hslot((uint32_t)w_des, t);
	if (!read_slot || !write_slot) {
		handle_free(t, r_des);
		handle_free(t, w_des);
		
		kfree(read_end);
		kfree(write_end);
		kfree(p);
		
		return PANUTIERRNO_PLAINERR;
	}

	read_slot->type = INODE_PIPE;
	read_slot->impl = read_end;
	read_slot->ops = &pipe_ops;
	read_slot->inode = nullptr;

	write_slot->type = INODE_PIPE;
	write_slot->impl = write_end;
	write_slot->ops = &pipe_ops;
	write_slot->inode = nullptr;

	*out_read = r_des;
	*out_write = w_des;
	return PANUTIERRNO_PLAINSUCCESS;
}