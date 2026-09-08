#include <kernel/handle/registry.h>
#include <string.h>

static void registry_destroy(inode_t* inode);

static inode_t inodes[REG_MAX_INODES];
static dirent_t dirents[REG_MAX_DIRENTS];
static inode_t* root;

inode_t* registry_inode_alloc(inode_type_t type) {
	for (int i = 0; i < REG_MAX_INODES; i++) {
		if (!inodes[i].in_use) {
			inodes[i].in_use = true;
			inodes[i].type = type;
			inodes[i].refcount = 1;
			inodes[i].impl = NULL;
			inodes[i].ops = NULL;
			inodes[i].children = NULL;
			inodes[i].fs_ops = NULL;
			inodes[i].fs_impl = NULL;
			return &inodes[i];
		}
	}
	
	return NULL;
}

static dirent_t* dirent_alloc(void) {
	for (int i = 0; i < REG_MAX_DIRENTS; i++) {
		if (!dirents[i].in_use) {
			dirents[i].in_use = true;
			dirents[i].next = NULL;
			dirents[i].inode = NULL;
			dirents[i].name[0] = '\0';
			return &dirents[i];
		}
	}
	
	return NULL;
}

// links name -> target into dir's children list. does not check for collisions
dirent_t* registry_linkdirent(inode_t* dir, const char* name, size_t len, inode_t* target) {
	if (len >= REG_MAX_NAME_LEN) {
		return NULL;
	}
	
	dirent_t* d = dirent_alloc();
	if (!d) {
		return NULL;
	}
	
	memcpy(d->name, name, len);
	d->name[len] = '\0';
	d->inode = target;
	d->next = dir->children;
	dir->children = d;
	target->refcount++;
	return d;
}

dirent_t* registry_finddirent(inode_t* dir, const char* name, size_t len) {
	for (dirent_t* d = dir->children; d; d = d->next) {
		if (strlen(d->name) == len && strncmp(d->name, name, len) == 0) {
			return d;
		}
	}
	
	return NULL;
}

void registry_init(void) {
	for (int i = 0; i < REG_MAX_INODES; i++) {
		inodes[i].in_use = false;
	}
	
	for (int i = 0; i < REG_MAX_DIRENTS; i++) {
		dirents[i].in_use = false;
	}

	root = registry_inode_alloc(INODE_DIR);
	// root is its own parent, by convention
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
		return NULL;
	}

	inode_t* current = (path[0] == '/') ? root : start;
	const char* p = (path[0] == '/') ? path + 1 : path;

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
			return NULL; // tried to descend into a non-directory
		}

		dirent_t* d = registry_finddirent(current, seg_start, len);
		inode_t* child = d ? d->inode : NULL;

		if (!child && current->fs_ops && current->fs_ops->lookup) {
			child = current->fs_ops->lookup(current->fs_impl, current, seg_start, len);
		}

		if (!child) {
			if (is_last && create_last) {
				inode_t* new_inode = registry_inode_alloc(create_type);
				if (!new_inode) {
					return NULL;
				}
				
				if (!registry_linkdirent(current, seg_start, len, new_inode)) {
					return NULL; // TODO: leaks new_inode on this path
				}
				
				if (create_type == INODE_DIR) {
					registry_linkdirent(new_inode, ".", 1, new_inode);
					registry_linkdirent(new_inode, "..", 2, current);
				}
				
				return new_inode; // last component, done
			}
			
			return NULL; // missing component
		} else if (is_last && create_last) {
			return NULL; // name collision
		}

		current = child;
		if (*p == '/') {
			p++;
		}
	}

	if (p > path + 1 && *(p - 1) == '/' && current->type != INODE_DIR) {
		return NULL;
	}

	return current;
}

int registry_mkdir(const char* path) {
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

	inode_t* parent;
	if (last == path) {
		parent = root;
	} else {
		char buf[128];
		size_t plen = (size_t)(last - path);
		if (plen >= sizeof(buf)) {
			return -1;
		}
		memcpy(buf, path, plen);
		buf[plen] = '\0';
		parent = registry_resolve(root, buf);
	}

	if (!parent || parent->type != INODE_DIR) {
		return -1;
	}

	if (parent->fs_ops && parent->fs_ops->create) {
		int ret = parent->fs_ops->create(parent->fs_impl, parent, last, namelen, INODE_DIR);
		if (ret < 0) {
			return ret;
		}
	}

	inode_t* n = walk(root, path, true, INODE_DIR);
	return n ? 0 : -1;
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
	if (n->fs_ops) {
		return -1; // already mounted
	}

	n->fs_ops = fs_ops;
	n->fs_impl = fs_impl;
	return 0;
}

int registry_unmount(const char* path) {
	if (!path) {
		return -1;
	}

	inode_t* n = walk(root, path, false, INODE_DIR);
	if (!n || n->type != INODE_DIR) {
		return -1;
	}
	if (!n->fs_ops) {
		return -1; // not mounted
	}

	n->fs_ops = NULL;
	n->fs_impl = NULL;
	return 0;
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
			if (d->inode != inode) {
				inode_unref(d->inode);
			}
			d->in_use = false;
			d = next;
		}
		inode->children = NULL;
	}

	inode->in_use = false;
}

dirent_t* registry_unlink(inode_t* dir, const char* name, size_t len) {
	if (!dir || dir->type != INODE_DIR || !name) {
		return NULL;
	}

	if (dir->fs_ops && dir->fs_ops->unlink) {
		int ret = dir->fs_ops->unlink(dir->fs_impl, dir, name, len);
		if (ret < 0) {
			return NULL;
		}
	}

	dirent_t* prev = NULL;
	dirent_t* curr = dir->children;

	while (curr) {
		if (strlen(curr->name) == len && strncmp(curr->name, name, len) == 0) {
			if (prev) {
				prev->next = curr->next;
			} else {
				dir->children = curr->next;
			}

			inode_unref(curr->inode);
			curr->in_use = false;
			curr->next = NULL;
			return curr;
		}
		prev = curr;
		curr = curr->next;
	}

	return NULL;
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