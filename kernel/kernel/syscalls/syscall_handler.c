#include <kernel/syscalls/syscall_handler.h>
#include <stddef.h>
#include <kernel/syscalls/syscall_no.h>
#include "handlers/handlers.h"

static syscall_handler_t syscall_handlers[] = {
	[SYSCALL_VGA] = sys_vga,
	[SYSCALL_EXIT] = sys_exit,
};

#define SYSCALL_COUNT (sizeof(syscall_handlers) / sizeof(syscall_handlers[0]))

uint32_t syscall_handler(uint32_t number, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
    if (number >= SYSCALL_COUNT || syscall_handlers[number] == NULL) {
        return (uint32_t)-1;
    }
    return syscall_handlers[number](arg1, arg2, arg3, arg4);
}