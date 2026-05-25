#include "syscall.h"
#include <stdint.h>
#include <stdlib.h>

#define MSR_SYSENTER_CS 0x174
#define MSR_SYSENTER_ESP 0x175
#define MSR_SYSENTER_EIP 0x176

extern uint32_t stack_top;
extern void sysentry_handler(void);

static void wrmsr(uint32_t msr, uint32_t value) {
	__asm__ __volatile__("wrmsr" :: "c"(msr), "a"(value), "d"(0));
}

static int is_sysenter_supported(void) {
    uint32_t edx;
    __asm__ __volatile__(
        "cpuid"
        : "=d"(edx)
        : "a"(1)
        : "ebx", "ecx"
    );
    return (edx >> 11) & 1;
}

void syscall_init(void) {
	if (!is_sysenter_supported()) {
		// no sysenter support, there will be no plans to support older CPUs, so just panic here.
		abort();
	}
	wrmsr(MSR_SYSENTER_CS, 0x08); // kernel code segment selector
	wrmsr(MSR_SYSENTER_ESP, (uint32_t)&stack_top);
	wrmsr(MSR_SYSENTER_EIP, (uint32_t)sysentry_handler);
}