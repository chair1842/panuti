#ifndef _KERNEL_HANDLE_PIPE_H
#define _KERNEL_HANDLE_PIPE_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/sched/task.h>

#define PIPE_BUFFER_SIZE 4096
#define PIPE_MAX_WAITERS 8

typedef struct pipe {
	uint8_t buffer[PIPE_BUFFER_SIZE];

	size_t capacity; // length of buffer
	size_t head; // index of next byte read
	size_t tail; // index of next byte written
	size_t count; // of bytes sitting in the buffer

	int refcount;

	task_t* readers_waiting[PIPE_MAX_WAITERS];
	int no_readers_waiting;

	task_t* writers_waiting[PIPE_MAX_WAITERS];
	int no_writers_waiting;

	bool write_end_closed;
	bool read_end_closed;
} pipe_t;

typedef struct pipe_end {
	pipe_t* pipe;
	bool is_write_end;
} pipe_end_t;

pipe_end_t* pipe_end_create(pipe_t* pipe, bool is_write_end);
int pipe_create_pair(task_t* t, int* out_read, int* out_write);

#endif