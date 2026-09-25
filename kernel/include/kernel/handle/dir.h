/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_HANDLE_DIR_H
#define _KERNEL_HANDLE_DIR_H

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <stddef.h>

typedef struct dir_handle {
	size_t cursor;
	dirent_t* next_inode;
} dir_handle_t;

extern const handle_ops_t dir_handle_ops;

#endif