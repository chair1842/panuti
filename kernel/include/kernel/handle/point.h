#ifndef KERNEL_POINT_H
#define KERNEL_POINT_H

#define MAX_POINT_WAITERS 8

struct task;

typedef struct {
	struct task* waiters[MAX_POINT_WAITERS];
	int waiter_count;
} point_t;

// creates a new point_t and registers it in the handle registry under path.
// returns 0 on success, negative on failure (registry full, name taken, or pool exhausted).
int point_create(const char* path);

#endif