#ifndef PANUTI_KERNEL_ARCH_I386_SYSCALL_HANDLERS_H
#define PANUTI_KERNEL_ARCH_I386_SYSCALL_HANDLERS_H
#include "../syscall.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <panuti/errno.h>
#include <panuti/syscall/syscallno.h>

#define USER_SPACE_BASE 0x08048000u
#define USER_SPACE_END  0xC0000000u

static inline bool is_user_ptr(const void* ptr) {
	uint32_t addr = (uint32_t)ptr;
	return addr >= USER_SPACE_BASE && addr < USER_SPACE_END;
}

static inline bool is_user_range(const void* buf, size_t len) {
	if (len == 0) return true;
	uint32_t start = (uint32_t)buf;
	uint32_t end = start + (uint32_t)len;
	return start >= USER_SPACE_BASE && end <= USER_SPACE_END && end > start;
}

// write to a handle
int32_t syshandler_write(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// exit the current process
int32_t syshandler_exit(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// open a handle
int32_t syshandler_open(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// read from a handle
int32_t syshandler_read(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// activate a handle
int32_t syshandler_activate(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// wait on a handle
int32_t syshandler_wait(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// close a handle
int32_t syshandler_close(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// mkdir in registry
int32_t syshandler_mkdir(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// create a point with the curr proc as the owner
int32_t syshandler_point_create(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// change the current working directory
int32_t syshandler_chdir(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// unlink a name from a directory
int32_t syshandler_unlink(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

#endif