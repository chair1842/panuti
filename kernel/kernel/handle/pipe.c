#include <kernel/handle/pipe.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
#include <kernel/sched/sched.h>
#include <stdalign.h>
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

static void pipe_wake_all(task_t** list, int* count) {
	for (int i = 0; i < *count; i++) {
		task_wake(list[i]);
	}
	
	*count = 0;
}

int pipe_read(void* impl, void* buf, size_t len) {
	pipe_end_t* end = impl;
	if (end->is_write_end) {
		return -1; // can't read from the write end
	}
	
	pipe_t* p = end->pipe;

	while (p->count == 0) {
		if (p->write_end_closed) {
			return 0; // EOF, nothing more will ever arrive
		}
		
		if (p->no_readers_waiting < PIPE_MAX_WAITERS) {
			p->readers_waiting[p->no_readers_waiting++] = sched_current();
		}
		
		task_block(sched_current());
		
		// resumes here once task_wake() is called on us
	}

	size_t n = pipe_read_raw(p, buf, len);
	pipe_wake_all(p->writers_waiting, &p->no_writers_waiting);

	return (int)n;
}

int pipe_write(void* impl, const void* buf, size_t len) {
	pipe_end_t* end = impl;
	if (!end->is_write_end) {
		return -1; // can't write to the read end
	}
	
	pipe_t* p = end->pipe;

	if (p->read_end_closed) {
		return -1; // broken pipe
	}

	while (p->count == p->capacity) {
		if (p->read_end_closed) {
			return -1; // reader disappeared while we waited
		}
		
		if (p->no_writers_waiting < PIPE_MAX_WAITERS) {
			p->writers_waiting[p->no_writers_waiting++] = sched_current();
		}
		
		task_block(sched_current());
	}

	size_t n = pipe_write_raw(p, buf, len);
	pipe_wake_all(p->readers_waiting, &p->no_readers_waiting);

	return (int)n;
}

int pipe_close(void* impl, struct task* self) {
	(void)self;
	pipe_end_t* end = impl;
	pipe_t* p = end->pipe;

	if (end->is_write_end) {
		p->write_end_closed = true;
		pipe_wake_all(p->readers_waiting, &p->no_readers_waiting);
	} else {
		p->read_end_closed = true;
		pipe_wake_all(p->writers_waiting, &p->no_writers_waiting);
	}

	p->refcount--;
	if (p->refcount == 0) {
		kfree(p);
	}

	kfree(end);
	return 0;
}

pipe_end_t* pipe_end_create(pipe_t* pipe, bool is_write_end) {
	pipe_end_t* end = kmalloc(sizeof(pipe_end_t), alignof(pipe_end_t));
	if (!end) {
		return NULL;
	}
	
	end->pipe = pipe;
	end->is_write_end = is_write_end;
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
	p->no_readers_waiting = 0;
	p->no_writers_waiting = 0;
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

	int r_des = handle_alloc(t);
	if (r_des < 0) {
		kfree(read_end);
		kfree(write_end);
		kfree(p);
		return PANUTIERRNO_NOFDS;
	}

	int w_des = handle_alloc(t);
	if (w_des < 0) {
		handle_free(t, r_des);
		kfree(read_end);
		kfree(write_end);
		kfree(p);
		return PANUTIERRNO_NOFDS;
	}

	t->handles[r_des].type = INODE_PIPE;
	t->handles[r_des].impl = read_end;
	t->handles[r_des].ops = &pipe_ops;
	t->handles[r_des].inode = NULL;

	t->handles[w_des].type = INODE_PIPE;
	t->handles[w_des].impl = write_end;
	t->handles[w_des].ops = &pipe_ops;
	t->handles[w_des].inode = NULL;

	*out_read = r_des;
	*out_write = w_des;
	return PANUTIERRNO_PLAINSUCCESS;
}