/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/mount.h>
#include <kernel/handle/registry.h>
#include <kernel/klog.h>

static mount_t mounts[REG_MAX_MOUNTS];
static uint32_t next_mount_id = 1;

void mount_init(void) {
	for (int i = 0; i < REG_MAX_MOUNTS; i++) {
		mounts[i].in_use = false;
		mounts[i].next = nullptr;
		mounts[i].refcount = 0;
	}
	next_mount_id = 1;
}

mount_t* mount_find(struct inode* mountpoint) {
	if (!mountpoint) {
		return nullptr;
	}
	for (int i = 0; i < REG_MAX_MOUNTS; i++) {
		if (mounts[i].in_use && mounts[i].mountpoint == mountpoint) {
			return &mounts[i];
		}
	}
	return nullptr;
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
	
	if (mountpoint->deleted) {
		return -1; // no longer in the namespace
	}

	mount_t* m = nullptr;
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
	m->next = nullptr;

	inode_ref(mountpoint);
	inode_ref(root);
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

	inode_t* root = m->root;
	inode_t* mountpoint_inode = m->mountpoint;

	if (root) {
		// the mounted root no longer hangs off anything. clear this before
		// the drop, so a destroy triggered by the last reference does not see
		// a stale mount
		root->mnt = nullptr;
	}
	if (mountpoint_inode) {
		// just in case a walk ever left a stale pointer on the cover
		mountpoint_inode->mnt = nullptr;
	}

	m->in_use = false;
	m->mountpoint = nullptr;
	m->root = nullptr;
	m->fs_ops = nullptr;
	m->fs_impl = nullptr;
	m->refcount = 0;
	m->next = nullptr;

	// drop the mount's own references. this has to go through inode_unref
	// rather than a bare decrement, or the slots are never returned
	if (root) {
		inode_unref(root);
	}
	if (mountpoint_inode) {
		inode_unref(mountpoint_inode);
	}

	return 0;
}
