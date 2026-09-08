#include <kernel/handle/registry.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <string.h>
#include <panuti/errno.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>

int32_t syshandler_link(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3; (void)a4;
	const char* target = (const char*)a1;
	const char* newpath = (const char*)a2;

	if (!kernel_is_user_ptr(target) || !kernel_is_user_ptr(newpath)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();

	inode_t* node = registry_resolve(t->cwd, target);
	if (!node) {
		return PANUTIERRNO_NOTFOUND;
	}

	// hard links to directories make loops that never end, so no
	if (node->type == INODE_DIR) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	inode_t* new_parent;
	const char* new_name;
	size_t new_len;
	if (registry_splitpath(t->cwd, newpath, &new_parent, &new_name, &new_len) != 0) {
		return PANUTIERRNO_NOTFOUND;
	}

	if (mount_find(new_parent) || new_parent->mnt) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	if (registry_finddirent(new_parent, new_name, new_len)) {
		return PANUTIERRNO_EXISTS;
	}

	if (!registry_linkdirent(new_parent, new_name, new_len, node)) {
		return PANUTIERRNO_PLAINERR;
	}

	return 0;
}