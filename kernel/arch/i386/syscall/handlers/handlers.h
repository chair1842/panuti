#ifndef PANUTI_KERNEL_ARCH_I386_SYSCALL_HANDLERS_H
#define PANUTI_KERNEL_ARCH_I386_SYSCALL_HANDLERS_H
#include "../syscall.h"
#include <stdint.h>
#include <panuti/errno.h>

// define syscall nos
#define SYSHANDLER_VGA 0
#define SYSHANDLER_EXIT 1

// put a char to the screen (temp)
int32_t syshandler_vga(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

// exit the current process
int32_t syshandler_exit(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

#endif