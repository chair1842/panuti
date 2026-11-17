/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_MEM_USR_H
#define _KERNEL_MEM_USR_H

#include <stddef.h>
#include <kernel/memman/layout.h>

#define USER_SPACE_BASE 0x08048000u
#define USER_SPACE_END  KERNEL_VIRT_OFFSET

bool kernel_is_user_ptr(const void* ptr);
bool kernel_is_user_range(const void* buf, size_t len);
// returns the length of a NUL-terminated user string if it fits entirely within
// user address space, or (size_t)-1 if it is not NUL-terminated in bounds.
size_t kernel_user_strlen(const void* ptr);
bool kernel_usrptr_verify(const void* ptr, size_t len);

#endif