/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
#include <kernel/klog.h>
#include <panuti/errno.h>
#include <limits.h>
#include <string.h>

static void registry_destroy(inode_t* inode);
static dirent_t* dirent_detach(inode_t* dir, const char* name, size_t len);
static void dirent_put(dirent_t* d, bool drop_inode);
static bool registry_dir_is_empty(inode_t* dir);

static bool is_dot_name(const char* name, size_t len) {
	return (len == 1 && name[0] == '.') || (len == 2 && name[0] == '.' && name[1] == '.');
}

static int inode_count;
static int dirent_count;
static inode_t* root;

inode_t* registry_inode_alloc(inode_type_t type) {
	if (inode_count > REG_MAX_INODES) {
		klog(KLOG_WARN, "registry: inode conut cap reached\n");
		return nullptr;
	}

	inode_t* in = kmalloc(sizeof(inode_t), alignof(inode_t));
	if (!in) {
		klog(KLOG_WARN, "registry: no more memory to allocate inode\n");
		return nullptr;
	}
	
	in->type = type;
	in->refcount = 1;
	in->deleted = false;
	in->impl = nullptr;
	in->ops = nullptr;
	in->children = nullptr;
	in->mnt = nullptr;
	
	inode_count++;
	
	return in;
}

static dirent_t* dirent_alloc(void) {
	if (dirent_count > REG_MAX_DIRENTS) {
		klog(KLOG_WARN, "registry: dirent count cap reached\n");
		return nullptr;
	}
	
	dirent_t* d = kmalloc(sizeof(dirent_t), alignof(dirent_t));
	if (!d) {
		klog(KLOG_WARN, "registry: no more memory to allocate dirent\n");
		return nullptr;
	}
	
	d->name_len = 0;
	d->refcount = 1;
	d->inode = nullptr;
	d->next = nullptr;
	
	return d;
}

void dirent_ref(dirent_t* d) {
	if (!d || !d->name) {
		return;
	}
	
	if (d->refcount == UINT16_MAX) {
		return; // one list reference plus one per open handle cannot reach this
	}
	
	d->refcount++;
}

static void dirent_put(dirent_t* d, bool drop_inode) {
	if (!d || !d->name) {
		return;
	}

	if (d->refcount > 0) {
		d->refcount--;
	}
	
	if (d->refcount != 0) {
		return;
	}
	
	dirent_count--;

	inode_t* target = drop_inode ? d->inode : nullptr;
	kfree(d->name);
	kfree(d);

	if (target) {
		inode_unref(target);
	}
}

void dirent_unref(dirent_t* d) {
	dirent_put(d, true);
}

// links name -> target into dir's children list. does not check for collisions
dirent_t* registry_linkdirent(inode_t* dir, const char* name, size_t len, inode_t* target) {
	if (len == 0 || len >= REG_MAX_NAME_LEN) {
		return nullptr;
	}
	
	// build the name copy before claiming a slot, so a claimed slot is never
	// briefly nameless and a failed allocation leaves the table untouched
	char* copy = kmalloc((uint32_t)len + 1, 1);
	if (!copy) {
		return nullptr;
	}
	
	memcpy(copy, name, len);
	copy[len] = '\0';

	dirent_t* d = dirent_alloc();
	if (!d) {
		kfree(copy);
		return nullptr;
	}
	
	d->name = copy;
	d->name_len = (uint16_t)len;
	d->inode = target;
	d->next = dir->children;
	dir->children = d;
	if (target != dir) {
		inode_ref(target);
	}
	
	return d;
}

dirent_t* registry_finddirent(inode_t* dir, const char* name, size_t len) {
	for (dirent_t* d = dir->children; d; d = d->next) {
		if (d->name_len == len && memcmp(d->name, name, len) == 0) {
			return d;
		}
	}
	
	return nullptr;
}

void registry_init(void) {
	mount_init();
	
	inode_count = 0;
	dirent_count = 0;

	root = registry_inode_alloc(INODE_DIR);
	registry_linkdirent(root, ".", 1, root);
	registry_linkdirent(root, "..", 2, root);
}

inode_t* registry_root(void) {
	return root;
}

// walks path component by component starting at start (or root, if
// path is absolute). if create_last is true, the final component is
// created as create_type instead of requiring it to already exist.
static inode_t* walk(inode_t* start, const char* path, bool create_last, inode_type_t create_type) {
	if (!path || path[0] == '\0') {
		return nullptr;
	}

	inode_t* current = (path[0] == '/') ? root : start;
	const char* p = (path[0] == '/') ? path + 1 : path;

	// if resolution *begins* at a mountpoint (a filesystem mounted over the
	// root, or over the caller's cwd), step into the mount so its tree is the
	// namespace the path gets resolved against. this also makes a bare "/"
	// resolve to the mounted root when the root is covered.
	if (!current->mnt) {
		mount_t* m = mount_find(current);
		if (m) {
			current = m->root;
			current->mnt = m;
		}
	}

	while (*p) {
		const char* seg_start = p;
		while (*p && *p != '/') p++;
		size_t len = (size_t)(p - seg_start);

		if (len == 0) {
			if (*p == '/') p++;
			continue; // double slash
		}

		bool is_last = (*p == '\0');

		if (current->type != INODE_DIR) {
			return nullptr; // tried to descend into a non-directory
		}

		mount_t* cmnt = current->mnt; // NULL -> registry tree, else we're inside a mount
		inode_t* child = nullptr;
		bool crossed_out = false;

		if (cmnt) {
			// inside a mounted filesystem: resolve through the mount
			if (len == 2 && seg_start[0] == '.' && seg_start[1] == '.' && current == cmnt->root) {
				// '..' at the mount root has no on-disk parent to follow, so it
				// crosses back over the boundary. it has to land on the parent of
				// the mountpoint, not on the mountpoint itself: returning the
				// cover would drop us inside the directory we are mounted over,
				// so '/mnt/..' could never climb back out to '/'. every namespace
				// directory gets a '..' pointing at its parent, so follow that.
				dirent_t* up = registry_finddirent(cmnt->mountpoint, "..", 2);
				child = up ? up->inode : cmnt->mountpoint;
				crossed_out = true;
			} else if (cmnt->fs_ops->lookup) {
				child = cmnt->fs_ops->lookup(cmnt->fs_impl, current, seg_start, len);
				if (child) {
					child->mnt = cmnt;
				}
			} else {
				return nullptr;
			}
		} else {
			// registry tree: look in the dirent list
			dirent_t* d = registry_finddirent(current, seg_start, len);
			child = d ? d->inode : nullptr;

			if (!child && is_last && create_last) {
				if (current->deleted) {
					return nullptr;
				}

				inode_t* new_inode = registry_inode_alloc(create_type);
				if (!new_inode) {
					return nullptr;
				}

				if (!registry_linkdirent(current, seg_start, len, new_inode)) {
					inode_unref(new_inode);
					return nullptr;
				}

				if (create_type == INODE_DIR) {
					// this amount of nesting is fucking criminal
					bool linked = registry_linkdirent(new_inode, ".", 1, new_inode) 
								&&
								registry_linkdirent(new_inode, "..", 2, current);
					
					if (!linked) {
						dirent_t* self = dirent_detach(new_inode, ".", 1);
						if (self) {
							dirent_put(self, self->inode != new_inode);
						}
						
						dirent_t* parent = dirent_detach(new_inode, "..", 2);
						if (parent) {
							dirent_put(parent, parent->inode != new_inode);
						}
						
						dirent_t* entry = dirent_detach(current, seg_start, len);
						if (entry) {
							dirent_unref(entry);
						}
						
						inode_unref(new_inode);
						return nullptr;
					}
				}

				inode_unref(new_inode);
				return new_inode;
			}
		}

		if (!child) {
			return nullptr; // missing component
		} else if (is_last && create_last) {
			return nullptr; // name collision
		}

		// descend into a mountpoint if this child is one (unless we just crossed
		// back out of the mount to its cover). the cover/mountpoint inode does
		// NOT get .mnt set -- only the mounted root does -- so a later detach
		// leaves no stale pointer on it.
		if (!crossed_out) {
			mount_t* m = mount_find(child);
			if (m) {
				child = m->root;
				child->mnt = m;
			}
		}

		current = child;
		if (*p == '/') {
			p++;
		}
	}

	if (p > path + 1 && *(p - 1) == '/' && current->type != INODE_DIR) {
		return nullptr;
	}

	return current;
}

int registry_mkdir_at(inode_t* start, const char* path) {
	if (!path || path[0] == '\0') {
		return -1;
	}

	size_t len = strlen(path);
	while (len > 1 && path[len - 1] == '/') {
		len--;
	}

	const char* last = path + len;
	while (last > path && *(last - 1) != '/') {
		last--;
	}

	size_t namelen = (size_t)((path + len) - last);
	if (namelen == 0) {
		return -1;
	}

	const inode_t* base = (path[0] == '/') ? root : start;

	inode_t* parent;
	if (last == path) {
		parent = (inode_t*)base;
	} else {
		char buf[128];
		size_t plen = (size_t)(last - path);
		if (plen >= sizeof(buf)) {
			return -1;
		}
		memcpy(buf, path, plen);
		buf[plen] = '\0';
		parent = registry_resolve((inode_t*)base, buf);
	}

	if (!parent || parent->type != INODE_DIR || parent->deleted) {
		return -1;
	}

	mount_t* pmnt = parent->mnt;
	if (pmnt) {
		// inside a mounted filesystem creation is the fs's job, and walk()
		// only ever creates in the registry tree. falling through to it after
		// the fs succeeded would re-resolve the name we just created and
		// report it as a collision, so return the fs result as-is
		if (!pmnt->fs_ops->create) {
			return -1;
		}

		return pmnt->fs_ops->create(pmnt->fs_impl, parent, last, namelen, INODE_DIR);
	}

	inode_t* n = walk((inode_t*)base, path, true, INODE_DIR);
	return n ? 0 : -1;
}

int registry_mkdir(const char* path) {
	return registry_mkdir_at(root, path);
}

int registry_mkfile_at(inode_t* start, const char* path) {
	if (!path || path[0] == '\0') {
		return -1;
	}

	size_t len = strlen(path);
	while (len > 1 && path[len - 1] == '/') {
		len--;
	}

	const char* last = path + len;
	while (last > path && *(last - 1) != '/') {
		last--;
	}

	size_t namelen = (size_t)((path + len) - last);
	if (namelen == 0) {
		return -1;
	}

	const inode_t* base = (path[0] == '/') ? root : start;

	inode_t* parent;
	if (last == path) {
		parent = (inode_t*)base;
	} else {
		char buf[128];
		size_t plen = (size_t)(last - path);
		if (plen >= sizeof(buf)) {
			return -1;
		}
		
		memcpy(buf, path, plen);
		buf[plen] = '\0';
		parent = registry_resolve((inode_t*)base, buf);
	}

	if (!parent || parent->type != INODE_DIR || parent->deleted) {
		return -1;
	}

	mount_t* pmnt = parent->mnt;
	if (!pmnt || !pmnt->fs_ops->create) {
		return -1;
	}

	return pmnt->fs_ops->create(pmnt->fs_impl, parent, last, namelen, INODE_FILE);
}

int registry_mkfile(const char* path) {
	return registry_mkfile_at(root, path);
}

int registry_add(const char* path, inode_type_t type, void* impl, const handle_ops_t* ops) {
	inode_t* n = walk(root, path, true, type);
	if (!n) {
		return -1;
	}
	
	n->impl = impl;
	n->ops = ops;
	return 0;
}

int registry_mount(const char* path, const fs_ops_t* fs_ops, void* fs_impl) {
	if (!fs_ops) {
		return -1;
	}

	inode_t* n = walk(root, path, false, INODE_DIR);
	if (!n || n->type != INODE_DIR) {
		return -1;
	}

	// generic mount: allocate a fresh registry dir to act as the FS root
	inode_t* r = registry_inode_alloc(INODE_DIR);
	if (!r) {
		return -1;
	}

	if (mount_attach(n, fs_ops, fs_impl, r) != 0) {
		inode_unref(r);
		return -1;
	}

	inode_unref(r);
	return 0;
}

int registry_unmount(const char* path) {
	if (!path) {
		return -1;
	}

	// the root has no parent dirent to resolve by name; the registry root
	// inode *is* the mountpoint, so find the covering mount directly
	if (path[0] == '/' && path[1] == '\0') {
		mount_t* m = mount_find(registry_root());
		if (!m) {
			return -1;
		}
		return mount_detach(m->mountpoint);
	}

	// resolve the mountpoint inode itself: walking the full path crosses into
	// the mount and hands back the mounted root, which mount_find() won't see
	inode_t* parent;
	const char* name;
	size_t namelen;
	if (registry_splitpath(root, path, &parent, &name, &namelen) != 0) {
		return -1;
	}

	dirent_t* d = registry_finddirent(parent, name, namelen);
	if (!d || d->inode->type != INODE_DIR) {
		return -1;
	}
	
	if (!mount_find(d->inode)) {
		return -1; // not mounted
	}

	return mount_detach(d->inode);
}

inode_t* registry_resolve(inode_t* start, const char* path) {
	return walk(start, path, false, INODE_DIR); 
}

inode_t* registry_find(const char* path) {
	return walk(root, path, false, INODE_DIR);
}

static void registry_destroy(inode_t* inode) {
	if (!inode || inode->refcount > 0) {
		return;
	}

	if (inode->type == INODE_DIR) {
		dirent_t* d = inode->children;
		while (d) {
			dirent_t* next = d->next;
			dirent_put(d, d->inode != inode);
			d = next;
		}
		
		inode->children = nullptr;
	}

	inode_count--;
	kfree(inode);
}

int registry_splitpath(
	inode_t* start,
	const char* path,
	inode_t** parent,
    const char** name,
    size_t* namelen
) {
	if (!path || path[0] == '\0') {
		return -1;
	}

	size_t len = strlen(path);
	while (len > 1 && path[len - 1] == '/') {
		len--;
	}

	// last points at the final component; anything before it is the parent dir
	const char* last = path + len;
	while (last > path && *(last - 1) != '/') {
		last--;
	}

	size_t nlen = (size_t)((path + len) - last);
	if (nlen == 0) {
		return -1;
	}

	inode_t* p;
	if (last == path) {
		p = start; // relative, no slashes at all: parent is where we stand
	} else {
		char buf[128];
		size_t plen = (size_t)(last - path);
		if (plen >= sizeof(buf)) {
			return -1;
		}
		
		memcpy(buf, path, plen);
		buf[plen] = '\0';
		p = registry_resolve(start, buf);
	}

	if (!p || p->type != INODE_DIR) {
		return -1;
	}

	*parent = p;
	*name = last;
	*namelen = nlen;
	return 0;
}

// unlinks name from dir's child list and drops the inode reference the list
// held. returns the removed dirent, or NULL if the name was not cached
static dirent_t* dirent_detach(inode_t* dir, const char* name, size_t len) {
	dirent_t* prev = nullptr;
	dirent_t* curr = dir->children;

	while (curr) {
		if (curr->name_len == len && memcmp(curr->name, name, len) == 0) {
			if (prev) {
				prev->next = curr->next;
			} else {
				dir->children = curr->next;
			}
			
			curr->next = nullptr;
			return curr;
		}
		prev = curr;
		curr = curr->next;
	}

	return nullptr;
}

// swaps a dirent's name in place, leaving the list and the inode reference
// alone. the new copy is built before the old one is released so a failed
// allocation leaves the entry exactly as it was
static bool dirent_rename(dirent_t* d, const char* name, size_t len) {
	if (len == 0 || len >= REG_MAX_NAME_LEN) {
		return false;
	}

	char* copy = kmalloc((uint32_t)len + 1, 1);
	if (!copy) {
		return false;
	}

	memcpy(copy, name, len);
	copy[len] = '\0';

	kfree(d->name);
	d->name = copy;
	d->name_len = (uint16_t)len;
	return true;
}

// the fs a directory belongs to, or NULL for the native registry tree. two
// directories only belong to the same filesystem if these match
static mount_t* dir_fs(inode_t* dir) {
	return dir->mnt;
}

int registry_unlink(inode_t* dir, const char* name, size_t len) {
	if (!dir || dir->type != INODE_DIR || !name) {
		return -1;
	}

	if (is_dot_name(name, len) || dir->deleted) {
		return PANUTIERRNO_BUSY;
	}

	dirent_t* d = registry_finddirent(dir, name, len);
	if (d && mount_find(d->inode)) {
		return PANUTIERRNO_BUSY;
	}

	if (!dir->mnt && d && d->inode->type == INODE_DIR &&
	    !registry_dir_is_empty(d->inode)) {
		return PANUTIERRNO_BUSY;
	}

	if (dir->mnt && dir->mnt->fs_ops->unlink) {
		int ret = dir->mnt->fs_ops->unlink(dir->mnt->fs_impl, dir, name, len);
		if (ret < 0) {
			return -1;
		}
	}

	d = dirent_detach(dir, name, len);
	if (!d) {
		return -1;
	}

	if (!dir->mnt && d->inode->type == INODE_DIR) {
		d->inode->deleted = true;
	}

	dirent_unref(d);
	return 0;
}

int registry_link(inode_t* target, inode_t* dir, const char* name, size_t len) {
	if (!target || !dir || !name || len == 0 || len >= REG_MAX_NAME_LEN) {
		return PANUTIERRNO_PLAINERR;
	}

	if (dir->type != INODE_DIR) {
		return PANUTIERRNO_NOTFOUND;
	}

	if (dir->deleted) {
		return PANUTIERRNO_BUSY;
	}

	if (is_dot_name(name, len) || target->type == INODE_DIR) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	// a hard link is a second name for one inode, so both ends have to live on
	// the same filesystem. linking a mounted file into the registry tree would
	// give it a name that vanishes at unmount, and linking across two mounts
	// would need an inode the kernel has no way to represent
	mount_t* m = dir_fs(dir);
	if (m != dir_fs(target)) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	if (m) {
		if (!m->fs_ops->link) {
			return PANUTIERRNO_UNSUPPORTEDOP;
		}

		int ret = m->fs_ops->link(m->fs_impl, target, dir, name, len);
		if (ret < 0) {
			return ret;
		}

		// cache the new name so the in-memory tree agrees with the disk. a
		// failure here is survivable: resolution inside a mount always goes
		// back to the fs, so the link is still reachable by path
		if (!registry_linkdirent(dir, name, len, target)) {
			klog(KLOG_WARN, "registry_link: could not cache new name in mounted dir\n");
		}

		return 0;
	}

	if (registry_finddirent(dir, name, len)) {
		return PANUTIERRNO_EXISTS;
	}

	if (!registry_linkdirent(dir, name, len, target)) {
		return PANUTIERRNO_PLAINERR;
	}

	return 0;
}

static bool registry_dir_is_empty(inode_t* dir) {
	for (dirent_t* d = dir->children; d; d = d->next) {
		bool self = (d->name_len == 1 && d->name[0] == '.');
		bool parent = (d->name_len == 2 && d->name[0] == '.' && d->name[1] == '.');

		if (!self && !parent) {
			return false;
		}
	}

	return true;
}

// true when dir is ancestor itself or lies below it in the native tree
static bool dir_has_ancestor(inode_t* dir, inode_t* ancestor) {
	while (dir) {
		if (dir == ancestor) {
			return true;
		}

		dirent_t* up = registry_finddirent(dir, "..", 2);
		if (!up || up->inode == dir) {
			return false;
		}
		dir = up->inode;
	}

	return false;
}

int registry_rename(inode_t* old_dir, const char* old_name, size_t old_len,
                    inode_t* new_dir, const char* new_name, size_t new_len) {
	if (!old_dir || !new_dir || !old_name || !new_name) {
		return PANUTIERRNO_PLAINERR;
	}

	if (old_dir->type != INODE_DIR || new_dir->type != INODE_DIR) {
		return PANUTIERRNO_NOTFOUND;
	}

	if (old_len == 0 || new_len == 0 ||
	    old_len >= REG_MAX_NAME_LEN || new_len >= REG_MAX_NAME_LEN) {
		return PANUTIERRNO_PLAINERR;
	}

	if (old_dir->deleted || new_dir->deleted) {
		return PANUTIERRNO_BUSY;
	}

	if (is_dot_name(old_name, old_len) || is_dot_name(new_name, new_len)) {
		return PANUTIERRNO_BUSY;
	}

	// renaming onto itself changes nothing, and must not be reported as a
	// collision with the entry we are about to move
	if (old_dir == new_dir && old_len == new_len &&
	    memcmp(old_name, new_name, old_len) == 0) {
		return 0;
	}

	dirent_t* old_entry = registry_finddirent(old_dir, old_name, old_len);
	if (old_entry && mount_find(old_entry->inode)) {
		return PANUTIERRNO_BUSY;
	}

	dirent_t* new_entry = registry_finddirent(new_dir, new_name, new_len);
	if (new_entry && mount_find(new_entry->inode)) {
		return PANUTIERRNO_BUSY;
	}

	mount_t* m = dir_fs(old_dir);
	if (m != dir_fs(new_dir)) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	if (m) {
		if (!m->fs_ops->rename) {
			return PANUTIERRNO_UNSUPPORTEDOP;
		}

		// the fs owns existence and collision checks on its own media
		int ret = m->fs_ops->rename(m->fs_impl, old_dir, old_name, old_len,
		                            new_dir, new_name, new_len);
		if (ret < 0) {
			return ret;
		}

		dirent_t* victim = dirent_detach(new_dir, new_name, new_len);
		if (victim) {
			dirent_unref(victim);
		}

		// ts is sooo dense
		dirent_t* d = registry_finddirent(old_dir, old_name, old_len);
		if (d) {
			if (old_dir == new_dir) {
				if (!dirent_rename(d, new_name, new_len)) {
					klog(KLOG_WARN, "registry_rename: could not re-cache renamed entry\n");
				}
			} else {
				dirent_t* moved = dirent_detach(old_dir, old_name, old_len);
				if (moved && !registry_linkdirent(new_dir, new_name, new_len, moved->inode)) {
					// put it back rather than leave the tree claiming a name
					// the old directory no longer has
					registry_linkdirent(old_dir, old_name, old_len, moved->inode);
					klog(KLOG_WARN, "registry_rename: could not cache new name in mounted dir\n");
					dirent_unref(moved);
				} else if (moved) {
					dirent_unref(moved);
				}
			}
		}

		return 0;
	}

	dirent_t* src = registry_finddirent(old_dir, old_name, old_len);
	if (!src) {
		return PANUTIERRNO_NOTFOUND;
	}

	inode_t* moved_inode = src->inode;
	bool moved_is_dir = (moved_inode->type == INODE_DIR);

	if (moved_is_dir && dir_has_ancestor(new_dir, moved_inode)) {
		return PANUTIERRNO_BUSY;
	}

	dirent_t* dst = registry_finddirent(new_dir, new_name, new_len);
	if (dst) {
		if (dst->inode == moved_inode) {
			return 0;
		}

		bool dst_dir = (dst->inode->type == INODE_DIR);

		if (moved_is_dir != dst_dir) {
			return PANUTIERRNO_UNSUPPORTEDOP;
		}

		if (dst_dir && !registry_dir_is_empty(dst->inode)) {
			return PANUTIERRNO_BUSY;
		}

		dst = dirent_detach(new_dir, new_name, new_len);
		if (!dst) {
			return PANUTIERRNO_PLAINERR;
		}

		if (dst_dir) {
			dst->inode->deleted = true;
		}
		
		dirent_unref(dst);
	}

	// link the new name first (bumps refcount), then drop the old one
	if (!registry_linkdirent(new_dir, new_name, new_len, moved_inode)) {
		return PANUTIERRNO_PLAINERR;
	}

	dirent_t* old = dirent_detach(old_dir, old_name, old_len);
	if (!old) {
		// cannot happen, we just found it, but roll back so the refcount the
		// link above claimed does not leak
		dirent_t* added = dirent_detach(new_dir, new_name, new_len);
		if (added) {
			dirent_unref(added);
		}
		
		return PANUTIERRNO_PLAINERR;
	}

	dirent_unref(old);

	if (moved_is_dir && old_dir != new_dir) {
		dirent_t* up = registry_finddirent(moved_inode, "..", 2);
		if (up && up->inode != new_dir) {
			inode_t* old_parent = up->inode;
			inode_ref(new_dir);
			up->inode = new_dir;
			inode_unref(old_parent);
		}
	}

	return 0;
}

void inode_ref(inode_t* inode) {
	if (!inode) {
		return;
	}
	
	if (inode->refcount < INT_MAX) {
		inode->refcount++;
	}
}

void inode_unref(inode_t* inode) {
	if (!inode) {
		return;
	}
	
	if (--inode->refcount != 0) {
		return;
	}
	
	registry_destroy(inode);
}
