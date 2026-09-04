#ifndef _KERNEL_BLOCK_UTILS_H
#define _KERNEL_BLOCK_UTILS_H

#include <kernel/block/block.h>

int block_read_bytes(block_dev_t* dev, uint64_t offset, uint32_t len, void* buf);

#endif