#include <kernel/block/block.h>
#include <kernel/handle/registry.h>
#include <kernel/handle/handle.h>
#include <kernel/memman/slab.h>
#include <stdalign.h>
#include <string.h>

typedef struct block_handle {
    block_dev_t* dev;
    size_t offset;
} block_handle_t;

static int block_read_op(void* impl, void* buf, size_t len) {
    block_handle_t* bh = (block_handle_t*)impl;
    block_dev_t* dev = bh->dev;
    uint32_t bs = dev->block_size;

    if (len == 0) return 0;

    uint64_t first_block = bh->offset / bs;
    uint64_t last_block = (bh->offset + len - 1) / bs;
    size_t num_blocks = (size_t)(last_block - first_block + 1);
    size_t buf_off = (size_t)(bh->offset % bs);

    void* tmp = kmalloc(num_blocks * bs, 1);
    if (!tmp) return 0;

    if (dev->ops->read(dev->impl, first_block, tmp, num_blocks) != 0) {
        kfree(tmp);
        return 0;
    }

    memcpy(buf, (uint8_t*)tmp + buf_off, len);
    kfree(tmp);

    bh->offset += len;
    return (int)len;
}

static int block_write_op(void* impl, const void* buf, size_t len) {
    block_handle_t* bh = (block_handle_t*)impl;
    block_dev_t* dev = bh->dev;
    uint32_t bs = dev->block_size;

    if (len == 0) return 0;

    uint64_t first_block = bh->offset / bs;
    uint64_t last_block = (bh->offset + len - 1) / bs;
    size_t num_blocks = (size_t)(last_block - first_block + 1);
    size_t buf_off = (size_t)(bh->offset % bs);
    size_t copy_len = len;

    // read-modify-write for partial blocks
    void* tmp = kmalloc(num_blocks * bs, 1);
    if (!tmp) return 0;

    int need_read = (buf_off != 0) || (copy_len < num_blocks * bs);
    if (need_read) {
        if (dev->ops->read(dev->impl, first_block, tmp, num_blocks) != 0) {
            kfree(tmp);
            return 0;
        }
    }

    memcpy((uint8_t*)tmp + buf_off, buf, copy_len);

    if (dev->ops->write(dev->impl, first_block, tmp, num_blocks) != 0) {
        kfree(tmp);
        return 0;
    }

    kfree(tmp);
    bh->offset += len;
    return (int)len;
}

static int block_activate_op(void* impl) {
    (void)impl;
    return -1;
}

static int block_rdy_op(void* impl) {
    (void)impl;
    return -1;
}

static int block_close_op(void* impl, struct task* self) {
    (void)self;
    kfree(impl);
    return 0;
}

const handle_ops_t block_handle_ops = {
    .read = block_read_op,
    .write = block_write_op,
    .activate = block_activate_op,
    .ready = block_rdy_op,
    .close = block_close_op,
};

void* block_open_handle(block_dev_t* dev) {
    block_handle_t* bh = kmalloc(sizeof(block_handle_t), alignof(block_handle_t));
    if (!bh) return NULL;
    bh->dev = dev;
    bh->offset = 0;
    return bh;
}

block_dev_t* block_register(const char* registry_path, const block_ops_t* ops, void* impl, uint32_t block_size, uint64_t block_count) {
    if (!ops || !registry_path || block_size == 0) {
        return NULL;
    }

    block_dev_t* dev = kmalloc(sizeof(block_dev_t), alignof(block_dev_t));
    if (!dev) return NULL;

    dev->ops = ops;
    dev->impl = impl;
    dev->block_size = block_size;
    dev->block_count = block_count;

    if (registry_add(registry_path, INODE_BLOCK, dev, &block_handle_ops) != 0) {
        kfree(dev);
        return NULL;
    }

    return dev;
}

block_dev_t* block_find(const char* registry_path) {
    inode_t* n = registry_find(registry_path);
    if (!n || n->type != INODE_BLOCK) return NULL;
    return (block_dev_t*)n->impl;
}
