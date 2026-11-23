/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/leaf.h>
#include <kernel/mem/usr.h>
#include <kernel/sched/sched.h>
#include <panuti/errno.h>
#include <string.h>

int syscall_leaf_begin(const char* user_path, syscall_leaf_t* out) {
	if (!out) {
		return PANUTIERRNO_INVALIDARG;
	}

	size_t path_len = kernel_user_strlen(user_path);
	if (path_len == (size_t)-1 || !kernel_usrptr_verify(user_path, path_len + 1)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (path_len == 0) {
		return PANUTIERRNO_INVALIDARG;
	}

	size_t end = path_len;
	while (end > 1 && user_path[end - 1] == '/') {
		end--;
	}

	const char* leaf = user_path + end;
	while (leaf > user_path && leaf[-1] != '/') {
		leaf--;
	}

	size_t leaf_len = (size_t)(user_path + end - leaf);
	if (leaf_len == 0 || leaf_len >= sizeof(out->name)) {
		return PANUTIERRNO_INVALIDARG;
	}

	size_t dir_len = (size_t)(leaf - user_path);
	char dirpart[REG_MAX_NAME_LEN];
	if (dir_len == 0) {
		dirpart[0] = '.';
		dirpart[1] = '\0';
	} else {
		if (dir_len >= sizeof(dirpart)) {
			return PANUTIERRNO_INVALIDARG;
		}

		memcpy(dirpart, user_path, dir_len);
		dirpart[dir_len] = '\0';
	}

	task_t* t = sched_current();
	if (!t) {
		return PANUTIERRNO_PLAINERR;
	}

	inode_t* dir = registry_resolve(t->cwd, dirpart);
	if (!dir || dir->type != INODE_DIR || dir->mnt) {
		return PANUTIERRNO_UNSUPPORTEDOP;
	}

	if (registry_finddirent(dir, leaf, leaf_len)) {
		return PANUTIERRNO_EXISTS;
	}

	memcpy(out->name, leaf, leaf_len);
	out->name[leaf_len] = '\0';
	out->name_len = leaf_len;
	out->dir = dir;
	return PANUTIERRNO_PLAINSUCCESS;
}
