#ifndef _KERNEL_BLOCK_BLOCK_H
#define _KERNEL_BLOCK_BLOCK_H

#include <stddef.h>
#include <stdint.h>
#include <kernel/handle/handle.h>

typedef struct block_dev block_dev_t;

typedef struct {
    int (*read)(void* impl, uint64_t block, void* buf, size_t count);
    int (*write)(void* impl, uint64_t block, const void* buf, size_t count);
    uint64_t (*count)(void* impl);
} block_ops_t;

typedef enum {
    BLOCK_OK = 0,
    BLOCK_ERR_INVAL = -1,
    BLOCK_ERR_IO = -2,
} block_err_t;

struct block_dev {
    const block_ops_t* ops;
    void* impl;
    uint32_t block_size;
    uint64_t block_count;
};

extern const handle_ops_t block_handle_ops;

block_dev_t* block_register(const char* registry_path, const block_ops_t* ops, void* impl, uint32_t block_size, uint64_t block_count);
block_dev_t* block_find(const char* registry_path);
void* block_open_handle(block_dev_t* dev);

#endif
