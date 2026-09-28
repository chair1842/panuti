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

void dir_handle_start(dir_handle_t* dh, dirent_t* entry);
void dir_handle_advance(dir_handle_t* dh);

extern const handle_ops_t dir_handle_ops;

#endif