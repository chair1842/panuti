#ifndef _KERNEL_ARCH_I386_SYSCALL_H
#define _KERNEL_ARCH_I386_SYSCALL_H
#include <stdint.h>

typedef int32_t (*syscall_handler_t)(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

void sysenter_init(void);
int32_t syscall_dispatch(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

#endif