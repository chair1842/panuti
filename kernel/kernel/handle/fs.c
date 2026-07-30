#include <kernel/handle/fs.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
#include <stdalign.h>

typedef struct fs_file {
    void* file_impl;
    const fs_ops_t* fs_ops;
    void* fs_impl;
    size_t offset;
} fs_file_t;

static int fs_file_read(void* impl, void* buf, size_t len) {
    fs_file_t* f = (fs_file_t*)impl;
    int ret = f->fs_ops->read(f->file_impl, buf, len, f->offset);
    if (ret > 0) {
        f->offset += (size_t)ret;
    }
    return ret;
}

static int fs_file_write(void* impl, const void* buf, size_t len) {
    fs_file_t* f = (fs_file_t*)impl;
    int ret = f->fs_ops->write(f->file_impl, buf, len, f->offset);
    if (ret > 0) {
        f->offset += (size_t)ret;
    }
    return ret;
}

static int fs_file_activate(void* impl) {
    (void)impl;
    return -1;
}

static int fs_file_rdy(void* impl) {
    (void)impl;
    return -1;
}

static int fs_file_close(void* impl, struct task* self) {
    (void)self;
    fs_file_t* f = (fs_file_t*)impl;
    if (f->fs_ops->close) {
        f->fs_ops->close(f->file_impl);
    }
    kfree(f);
    return 0;
}

const handle_ops_t fs_file_ops = {
    .read = fs_file_read,
    .write = fs_file_write,
    .activate = fs_file_activate,
    .ready = fs_file_rdy,
    .close = fs_file_close,
};

void* fs_open_file(void* fs_impl, const fs_ops_t* fs_ops, struct inode* node) {
    if (!fs_ops->open) {
        return NULL;
    }

    void* file_impl = fs_ops->open(fs_impl, node);
    if (!file_impl) {
        return NULL;
    }

    fs_file_t* f = kmalloc(sizeof(fs_file_t), alignof(fs_file_t));
    if (!f) {
        if (fs_ops->close) {
            fs_ops->close(file_impl);
        }
        return NULL;
    }

    f->file_impl = file_impl;
    f->fs_ops = fs_ops;
    f->fs_impl = fs_impl;
    f->offset = 0;

    return f;
}
