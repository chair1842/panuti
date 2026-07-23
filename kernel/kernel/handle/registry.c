#include <kernel/handle/registry.h>
#include <string.h>

static inode_t inodes[MAX_INODES];
static dirent_t dirents[MAX_DIRENTS];
static inode_t* root;

static inode_t* inode_alloc(inode_type_t type) {
	for (int i = 0; i < MAX_INODES; i++) {
		if (!inodes[i].in_use) {
			inodes[i].in_use = true;
			inodes[i].type = type;
			inodes[i].refcount = 0;
			inodes[i].impl = NULL;
			inodes[i].ops = NULL;
			inodes[i].children = NULL;
			return &inodes[i];
		}
	}
	
	return NULL;
}

static dirent_t* dirent_alloc(void) {
	for (int i = 0; i < MAX_DIRENTS; i++) {
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
static dirent_t* link_dirent(inode_t* dir, const char* name, size_t len, inode_t* target) {
	if (len >= MAX_NAME_LEN) {
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

static dirent_t* find_dirent(inode_t* dir, const char* name, size_t len) {
	for (dirent_t* d = dir->children; d; d = d->next) {
		if (strlen(d->name) == len && strncmp(d->name, name, len) == 0) {
			return d;
		}
	}
	
	return NULL;
}

void registry_init(void) {
	for (int i = 0; i < MAX_INODES; i++) {
		inodes[i].in_use = false;
	}
	
	for (int i = 0; i < MAX_DIRENTS; i++) {
		dirents[i].in_use = false;
	}

	root = inode_alloc(INODE_DIR);
	// root is its own parent, by convention
	link_dirent(root, ".", 1, root);
	link_dirent(root, "..", 2, root);
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

		dirent_t* d = find_dirent(current, seg_start, len);

		if (!d) {
			if (is_last && create_last) {
				inode_t* new_inode = inode_alloc(create_type);
				if (!new_inode) {
					return NULL;
				}
				
				if (!link_dirent(current, seg_start, len, new_inode)) {
					return NULL; // TODO: leaks new_inode on this path
				}
				
				if (create_type == INODE_DIR) {
					link_dirent(new_inode, ".", 1, new_inode);
					link_dirent(new_inode, "..", 2, current);
				}
				
				return new_inode; // last component, done
			}
			
			return NULL; // missing component
		} else if (is_last && create_last) {
			return NULL; // name collision
		}

		current = d->inode;
		if (*p == '/') {
			p++;
		}
	}

	return current;
}

int registry_mkdir(const char* path) {
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

inode_t* registry_resolve(inode_t* start, const char* path) {
	return walk(start, path, false, INODE_DIR); 
}

inode_t* registry_find(const char* path) {
	return walk(root, path, false, INODE_DIR);
}