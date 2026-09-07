#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/handle/fs.h>
#include <kernel/block/block.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include <stdint.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>

int32_t syshandler_open(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	if (!kernel_is_user_ptr(path)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();

	inode_t* n = registry_resolve(t->cwd, path);
	if (!n) {
		return PANUTIERRNO_NOTFOUND;
	}
	if (n->type == INODE_DIR) {
		return PANUTIERRNO_UNSUPPORTEDOP; // no directory-open semantics yet
	}

	int des = handle_alloc(t);
	if (des < 0) {
		return PANUTIERRNO_NOFDS;
	}

	if (n->type == INODE_BLOCK) {
		void* bh = block_open_handle((block_dev_t*)n->impl);
		if (!bh) {
			handle_free(t, des);
			return PANUTIERRNO_PLAINERR;
		}
		t->handles[des].type = n->type;
		t->handles[des].impl = bh;
		t->handles[des].ops = &block_handle_ops;
		t->handles[des].inode = n;
		n->refcount++;
		return des;
	}

	if (n->fs_ops) {
		void* file_impl = fs_open_file(n->fs_impl, n->fs_ops, n);
		if (!file_impl) {
			handle_free(t, des);
			return PANUTIERRNO_PLAINERR;
		}
		t->handles[des].type = n->type;
		t->handles[des].impl = file_impl;
		t->handles[des].ops = &fs_file_ops;
		t->handles[des].inode = n;
		n->refcount++;
		return des;
	}

	t->handles[des].type = n->type;
	t->handles[des].impl = n->impl;
	t->handles[des].ops = n->ops;
	t->handles[des].inode = n;

	n->refcount++;
	return des;
}