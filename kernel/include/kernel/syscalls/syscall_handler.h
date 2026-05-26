#ifndef _KERNEL_SYSCALLS_SYSCALL_HANDLER_H
#define _KERNEL_SYSCALLS_SYSCALL_HANDLER_H

#include <stdint.h>

typedef uint32_t (*syscall_handler_t)(uint32_t, uint32_t, uint32_t, uint32_t);

uint32_t syscall_handler(uint32_t syscall_no, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);

#endif