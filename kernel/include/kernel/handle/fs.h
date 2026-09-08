#ifndef _KERNEL_HANDLE_FS_H
#define _KERNEL_HANDLE_FS_H

#include <stddef.h>
#include <stdint.h>
#include <kernel/handle/inode_type.h>
#include <kernel/handle/handle.h>

struct inode;

typedef struct fs_ops {
	struct inode* (*lookup)(void* fs_impl, struct inode* dir, const char* name, size_t len);
	int (*create)(void* fs_impl, struct inode* dir, const char* name, size_t len, inode_type_t type);
	int (*unlink)(void* fs_impl, struct inode* dir, const char* name, size_t len);
	void* (*open)(void* fs_impl, struct inode* node);
	int (*read)(void* file_impl, void* buf, size_t len, size_t offset);
	int (*write)(void* file_impl, const void* buf, size_t len, size_t offset);
	void (*close)(void* file_impl);
	void (*finish)(void* fs_impl);
} fs_ops_t;

extern const handle_ops_t fs_file_ops;

void* fs_open_file(void* fs_impl, const fs_ops_t* fs_ops, struct inode* node);

#endif
