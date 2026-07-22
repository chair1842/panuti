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

#endif