#ifndef _KERNEL_HANDLE_REGISTRY_H
#define _KERNEL_HANDLE_REGISTRY_H

#include <kernel/handle/handle.h>
#include <stdbool.h>

#define MAX_REGISTERED 64
#define MAX_PATH_LEN 64

typedef struct {
	char path[MAX_PATH_LEN];
	handle_type_t type;
	void* impl;
	const handle_ops_t* ops;
	bool in_use;
} registry_entry_t;

int registry_add(const char* path, handle_type_t type, void* impl, const handle_ops_t* ops);
registry_entry_t* registry_find(const char* path);

#endif