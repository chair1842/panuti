#include "syscall.h"
#include "../msr.h"

#define KERNEL_CODE_SEL 0x08

extern void sysenter_entry(void);
extern uint32_t stack_top;

void sysenter_init(void) {
	wrmsr32(MSR_SYSENTER_CS, KERNEL_CODE_SEL);
	wrmsr32(MSR_SYSENTER_ESP, (uint32_t)&stack_top);
	wrmsr32(MSR_SYSENTER_EIP, (uint32_t)sysenter_entry);
}