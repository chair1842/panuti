#ifndef _KERNEL_BOOT_MODULE_H
#define _KERNEL_BOOT_MODULE_H
#include <stdint.h>
#include <stddef.h>

// Returns the raw bytes of the boot-provided init module (however the arch
// layer actually obtained them -- Multiboot module, embedded blob, etc).
// Returns a kernel-accessible pointer via *out_data and its size via
// *out_size. Returns 0 on success, -1 if no init module is available.
int kernel_get_init_module(const void** out_data, size_t* out_size);

#endif