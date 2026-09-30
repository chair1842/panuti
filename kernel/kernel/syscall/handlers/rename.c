/* SPDX-License-Identifier: GPL-2.0-or-later */

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

	if (!kernel_is_user_ptr(oldpath) || !kernel_is_user_ptr(newpath) ||
	    kernel_user_strlen(oldpath) == (size_t)-1 || kernel_user_strlen(newpath) == (size_t)-1) {
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

	// the registry decides whether this stays in its in-memory tree or goes
	// down to the backing filesystem
	return registry_rename(old_parent, old_name, old_len, new_parent, new_name, new_len);
}
