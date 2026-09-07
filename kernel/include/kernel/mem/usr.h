#ifndef _KERNEL_MEM_USR_H
#define _KERNEL_MEM_USR_H

#include <stdbool.h>
#include <stddef.h>

bool kernel_is_user_ptr(const void* ptr);
bool kernel_is_user_range(const void* buf, size_t len);

#endif