#ifndef _KERNEL_HANDLE_REGISTRY_H
#define _KERNEL_HANDLE_REGISTRY_H

#include <kernel/handle/handle.h>
#include <stdbool.h>
#include <kernel/handle/inode_type.h>

#define MAX_INODES 128
#define MAX_DIRENTS 256
#define MAX_NAME_LEN 32

typedef struct dirent {
	char name[MAX_NAME_LEN];
	struct inode* inode;
	struct dirent* next;
	bool in_use;
} dirent_t;

typedef struct inode {
	inode_type_t type;
	int refcount;
	bool in_use;

	// leaf-only
	void* impl;
	const handle_ops_t* ops;

	// dir-only
	dirent_t* children;
} inode_t;

void registry_init(void);
int registry_mkdir(const char* path);
int registry_add(const char* path, inode_type_t type, void* impl, const handle_ops_t* ops);
inode_t* registry_resolve(inode_t* start, const char* path);
inode_t* registry_find(const char* path);
inode_t* registry_root(void);

#endif