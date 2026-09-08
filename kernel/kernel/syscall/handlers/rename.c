#include <kernel/handle/registry.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <string.h>
#include <panuti/errno.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>

int32_t syshandler_rename(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3; (void)a4;
	const char* oldpath = (const char*)a1;
	const char* newpath = (const char*)a2;

	if (!kernel_is_user_ptr(oldpath) || !kernel_is_user_ptr(newpath)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();

	inode_t* old_parent;
	const char* old_name;
	size_t old_len;
	if (registry_splitpath(t->cwd, oldpath, &old_parent, &old_name, &old_len) != 0) {
		return PANUTIERRNO_NOTFOUND;
	}

	inode_t* new_parent;
	const char* new_name;
	size_t new_len;
	if (registry_splitpath(t->cwd, newpath, &new_parent, &new_name, &new_len) != 0) {
		return PANUTIERRNO_NOTFOUND;
	}

	// shuffling names around on disk is the filesystem's job
	if (old_parent->fs_ops || new_parent->fs_ops) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	dirent_t* src = registry_finddirent(old_parent, old_name, old_len);
	if (!src) {
		return PANUTIERRNO_NOTFOUND;
	}

	// same dir, same name: nothing to do, and certainly not an error
	if (old_parent == new_parent && old_len == new_len &&
	    strncmp(old_name, new_name, old_len) == 0) {
		return 0;
	}

	if (registry_finddirent(new_parent, new_name, new_len)) {
		return PANUTIERRNO_EXISTS;
	}

	// link the new name first (bumps refcount), then drop the old one
	if (!registry_linkdirent(new_parent, new_name, new_len, src->inode)) {
		return PANUTIERRNO_PLAINERR;
	}

	if (!registry_unlink(old_parent, old_name, old_len)) {
		// this should never happen since we just found it, but roll back
		// anyway so the refcount doesn't leak
		registry_unlink(new_parent, new_name, new_len);
		return PANUTIERRNO_PLAINERR;
	}

	return 0;
}