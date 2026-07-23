#ifndef KERNEL_HANDLE_H
#define KERNEL_HANDLE_H

#include <stddef.h>
#include <stdint.h>
#include <kernel/handle/inode_type.h>

struct task;

typedef struct {
	int (*read)(void* impl, void* buf, size_t len);
	int (*write)(void* impl, const void* buf, size_t len);
	int (*activate)(void* impl);
	int (*wait)(void* impl, struct task* self);
	int (*close)(void* impl, struct task* self);
} handle_ops_t;

typedef struct {
	inode_type_t type;          // was handle_type_t — now unified
	void* impl;
	const handle_ops_t* ops;
} handle_t;

#define MAX_HANDLES 32

int op_not_supported_rw(void* impl, void* buf, size_t len);
int op_not_supported_w(void* impl, const void* buf, size_t len);
int op_not_supported_act(void* impl);
int op_not_supported_wait(void* impl, struct task* self);
int op_not_supported_close(void* impl, struct task* self);

int handle_alloc(struct task* t);
void handle_free(struct task* t, int fd);

#endif