/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_SYSCALL_LEAF_H
#define _KERNEL_SYSCALL_LEAF_H

#include <kernel/handle/registry.h>
#include <stddef.h>

typedef struct {
	inode_t* dir;
	char name[REG_MAX_NAME_LEN];
	size_t name_len;
} syscall_leaf_t;

int syscall_leaf_begin(const char* user_path, syscall_leaf_t* out);

#endif
