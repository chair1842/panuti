#include <kernel/handle/mount.h>
#include <kernel/handle/registry.h>
#include <kernel/klog.h>

static mount_t mounts[REG_MAX_MOUNTS];
static uint32_t next_mount_id = 1;

void mount_init(void) {
	for (int i = 0; i < REG_MAX_MOUNTS; i++) {
		mounts[i].in_use = false;
		mounts[i].next = NULL;
		mounts[i].refcount = 0;
	}
	next_mount_id = 1;
}

mount_t* mount_find(struct inode* mountpoint) {
	if (!mountpoint) {
		return NULL;
	}
	for (int i = 0; i < REG_MAX_MOUNTS; i++) {
		if (mounts[i].in_use && mounts[i].mountpoint == mountpoint) {
			return &mounts[i];
		}
	}
	return NULL;
}

int mount_attach(struct inode* mountpoint, const fs_ops_t* fs_ops, void* fs_impl, struct inode* root) {
	if (!mountpoint || !fs_ops || !root) {
		return -1;
	}
	if (mount_find(mountpoint)) {
		return -1; // already a mountpoint
	}
	// when a path resolves into an existing mount it hands back the mount's
	// root inode (which carries .mnt), so mounting over it must be rejected too
	if (mountpoint->mnt) {
		return -1; // inside an existing mount
	}

	mount_t* m = NULL;
	for (int i = 0; i < REG_MAX_MOUNTS; i++) {
		if (!mounts[i].in_use) {
			m = &mounts[i];
			break;
		}
	}
	if (!m) {
		return -1; // mount table full
	}

	m->in_use = true;
	m->id = next_mount_id++;
	m->mountpoint = mountpoint;
	m->root = root;
	m->fs_ops = fs_ops;
	m->fs_impl = fs_impl;
	m->refcount = 0;
	m->next = NULL;

	root->refcount++;
	klog(KLOG_INFO, "mount: id=%u mp=%p root=%p\n", m->id, mountpoint, root);
	return 0;
}

int mount_detach(struct inode* mountpoint) {
	mount_t* m = mount_find(mountpoint);
	if (!m) {
		return -1; // not mounted
	}
	if (m->refcount > 0) {
		return -1; // in use; can't tear down while referenced
	}

	if (m->fs_ops->finish) {
		m->fs_ops->finish(m->fs_impl);
	}
	if (m->root) {
		// drop the mount's own reference on the root inode
		if (m->root->refcount > 0) {
			m->root->refcount--;
		}
		// the mounted root no longer hangs off anything
		m->root->mnt = NULL;
	}
	if (m->mountpoint) {
		// just in case a walk ever left a stale pointer on the cover
		m->mountpoint->mnt = NULL;
	}

	m->in_use = false;
	m->mountpoint = NULL;
	m->root = NULL;
	m->fs_ops = NULL;
	m->fs_impl = NULL;
	m->refcount = 0;
	m->next = NULL;
	return 0;
}
