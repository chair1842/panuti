#ifndef _KERNEL_HANDLE_REGISTRY_H
#define _KERNEL_HANDLE_REGISTRY_H

#include <kernel/handle/handle.h>
#include <kernel/handle/fs.h>
#include <kernel/handle/mount.h>
#include <stdbool.h>
#include <kernel/handle/inode_type.h>

#define REG_MAX_INODES 1024
#define REG_MAX_DIRENTS 2048
#define REG_MAX_NAME_LEN 256

typedef struct dirent {
	char name[REG_MAX_NAME_LEN];
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

	// filesystem attachment: if non-NULL, this inode lives inside a mounted
	// filesystem (fabricated by it), and `mnt` identifies the mount.
	struct mount* mnt;
} inode_t;

void registry_init(void);
int registry_mkdir(const char* path);
int registry_add(const char* path, inode_type_t type, void* impl, const handle_ops_t* ops);
inode_t* registry_resolve(inode_t* start, const char* path);
inode_t* registry_find(const char* path);
inode_t* registry_root(void);
inode_t* registry_inode_alloc(inode_type_t type);
dirent_t* registry_unlink(inode_t* dir, const char* name, size_t len);
void inode_unref(inode_t* inode);
int registry_mount(const char* path, const fs_ops_t* fs_ops, void* fs_impl);
int registry_unmount(const char* path);
dirent_t* registry_linkdirent(inode_t* dir, const char* name, size_t len, inode_t* target);
dirent_t* registry_finddirent(inode_t* dir, const char* name, size_t len);
int registry_splitpath(inode_t* start, const char* path, inode_t** parent,
                       const char** name, size_t* namelen);

#endif