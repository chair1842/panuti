#ifndef _KERNEL_SYSCALLS_HANDLERS_HANDLERS_H
#define _KERNEL_SYSCALLS_HANDLERS_HANDLERS_H

#include <stdint.h>

uint32_t sys_vga(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_exit(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);

#endif