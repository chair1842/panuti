#include "syscall.h"
#include "../msr.h"
#include <panuti/errno.h>
#include "handlers/handlers.h"

#define KERNEL_CODE_SEL 0x08

extern void sysenter_entry(void);
extern uint32_t stack_top; 

// decoration!! Syscall dispatch table
syscall_handler_t syscall_handlers[256] = {
	[SYSHANDLER_VGA] = syshandler_vga,
	[SYSHANDLER_EXIT] = syshandler_exit,
};

void sysenter_init(void) {
	wrmsr32(MSR_SYSENTER_CS, KERNEL_CODE_SEL);
	wrmsr32(MSR_SYSENTER_ESP, (uint32_t)&stack_top);
	wrmsr32(MSR_SYSENTER_EIP, (uint32_t)sysenter_entry);
}

int32_t syscall_dispatch(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	if (num >= 256 || !syscall_handlers[num]) {
		return PANUTIERRNO_INVALIDSYSCALL;
	}
	
	return syscall_handlers[num](a1, a2, a3, a4);
}