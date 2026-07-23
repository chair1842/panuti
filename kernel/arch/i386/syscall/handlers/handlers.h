#ifndef PANUTI_KERNEL_ARCH_I386_SYSCALL_HANDLERS_H
#define PANUTI_KERNEL_ARCH_I386_SYSCALL_HANDLERS_H
#include "../syscall.h"
#include <stdint.h>
#include <panuti/errno.h>
#include <panuti/syscall/syscallno.h>

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

// find entry in registry
int32_t syshandler_regfind(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// get registry root
int32_t syshandler_regroot(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

#endif