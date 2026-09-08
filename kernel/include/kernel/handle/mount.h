#ifndef _KERNEL_HANDLE_MOUNT_H
#define _KERNEL_HANDLE_MOUNT_H

#include <stdbool.h>
#include <stdint.h>
#include <kernel/handle/fs.h>

#define REG_MAX_MOUNTS 64

struct inode;

// A mounted filesystem. Binds a mountpoint inode to the root inode of a
// filesystem's namespace plus the fs ops/impl that drive it. The mount is a
// first-class object so it can be created, refcounted, queried, and torn down.
typedef struct mount {
	bool in_use;
	uint32_t id;
	struct inode* mountpoint;   // inode this filesystem hangs under
	struct inode* root;         // root inode of the mounted namespace
	const fs_ops_t* fs_ops;
	void* fs_impl;
	uint32_t refcount;
	struct mount* next;         // reserved for future stacked overlays
} mount_t;

void mount_init(void);
int mount_attach(struct inode* mountpoint, const fs_ops_t* fs_ops, void* fs_impl, struct inode* root);
int mount_detach(struct inode* mountpoint);
mount_t* mount_find(struct inode* mountpoint);

#endif
