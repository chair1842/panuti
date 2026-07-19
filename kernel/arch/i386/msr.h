#ifndef _KERNEL_ARCH_I386_MSR_H
#define _KERNEL_ARCH_I386_MSR_H
#include <stdint.h>

#define MSR_SYSENTER_CS 0x174
#define MSR_SYSENTER_ESP 0x175
#define MSR_SYSENTER_EIP 0x176

void rdmsr(uint32_t msr, uint32_t* lo, uint32_t* hi);
void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi);
void wrmsr32(uint32_t msr, uint32_t value);

#endif