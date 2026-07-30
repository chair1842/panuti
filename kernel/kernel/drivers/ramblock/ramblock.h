#ifndef _KERNEL_DRIVERS_RAMBLOCK_H
#define _KERNEL_DRIVERS_RAMBLOCK_H

#include <stdint.h>
#include <stddef.h>

void ramblock_init(const char* path, uint32_t block_size, uint64_t block_count);

#endif
