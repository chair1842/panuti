/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/fs.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
#include <panuti/errno.h>
#include <panuti/syscall/seek.h>

typedef struct fs_file {
	void* file_impl;
	const fs_ops_t* fs_ops;
	void* fs_impl;
	struct inode* node;
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

static int fs_file_seek(void* impl, int whence, int64_t offset) {
	fs_file_t* f = (fs_file_t*)impl;

	int64_t size = -1;
	if (f->fs_ops->size && f->node) {
		size = f->fs_ops->size(f->fs_impl, f->node);
	}

	int64_t base;
	switch (whence) {
	case SEEK_SET:
		base = 0;
		break;
	case SEEK_CUR:
		base = (int64_t)f->offset;
		break;
	case SEEK_END:
		if (size < 0) {
			return PANUTIERRNO_UNSUPPORTEDOP;
		}
		base = size;
		break;
	default:
		return PANUTIERRNO_INVALIDARG;
	}

	int64_t pos;
	if (offset > 0 && base > INT64_MAX - offset) {
		pos = INT64_MAX;
	} else {
		pos = base + offset;
	}
	if (pos < 0) {
		pos = 0;
	}

	uint64_t np = (uint64_t)pos;
	if (size >= 0 && np > (uint64_t)size) {
		np = (uint64_t)size;
	}
	if (np > SIZE_MAX) {
		np = SIZE_MAX;
	}

	f->offset = (size_t)np;
	return 0;
}

static int fs_file_resize(void* impl, uint64_t new_size) {
	fs_file_t* f = (fs_file_t*)impl;
	if (f->fs_ops->resize) {
		return f->fs_ops->resize(f->file_impl, new_size);
	}

	return PANUTIERRNO_UNSUPPORTEDOP;
}

const handle_ops_t fs_file_ops = {
	.read = fs_file_read,
	.write = fs_file_write,
	.resize = fs_file_resize,
	.seek = fs_file_seek,
	.activate = fs_file_activate,
	.ready = fs_file_rdy,
	.close = fs_file_close,
};

void* fs_open_file(void* fs_impl, const fs_ops_t* fs_ops, struct inode* node) {
	if (!fs_ops->open) {
		return nullptr;
	}

	void* file_impl = fs_ops->open(fs_impl, node);
	if (!file_impl) {
		return nullptr;
	}

	fs_file_t* f = kmalloc(sizeof(fs_file_t), alignof(fs_file_t));
	if (!f) {
		if (fs_ops->close) {
			fs_ops->close(file_impl);
		}
		
		return nullptr;
	}

	f->file_impl = file_impl;
	f->fs_ops = fs_ops;
	f->fs_impl = fs_impl;
	f->node = node;
	f->offset = 0;

	return f;
}
