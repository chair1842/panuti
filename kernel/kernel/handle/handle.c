/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <panuti/inode_type.h>
#include <kernel/handle/handle.h>
#include <panuti/errno.h>
#include <kernel/sched/task.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
#include <string.h>

#define FD_INITIAL_CAP 16

int op_not_supported_rw(void* impl, void* buf, size_t len) {
	(void)impl; (void)buf; (void)len;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

int op_not_supported_w(void* impl, const void* buf, size_t len) {
	(void)impl; (void)buf; (void)len;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

int op_not_supported_act(void* impl) {
	(void)impl;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

int op_not_supported_rdy(void* impl) {
	(void)impl;
	return -1;
}

int op_not_supported_close(void* impl, struct task* self) {
	(void)impl; (void)self;
	return 0;
}

int op_not_supported_resize(void* impl, uint64_t new_size) {
	(void)impl; (void)new_size;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

int op_not_supported_seek(void* impl, int whence, int64_t offset) {
	(void)impl; (void)whence; (void)offset;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

int op_seek_ignore(void* impl, int whence, int64_t offset) {
	(void)impl; (void)whence; (void)offset;
	return 0;
}

static int fdtable_grow(task_t* t) {
	fdtable_t* fdt = &t->fdtable;

	if (fdt->cap >= MAX_HANDLES) {
		return -1;
	}

	uint32_t new_cap = fdt->cap ? fdt->cap * 2 : FD_INITIAL_CAP;
	if (new_cap > MAX_HANDLES) {
		new_cap = MAX_HANDLES;
	}

	handle_t* new_table = kmalloc(new_cap * sizeof(handle_t), alignof(handle_t));
	if (!new_table) {
		return -1;
	}

	uint32_t new_openb_bytes = (new_cap + 7) / 8;
	uint8_t* new_openb = kmalloc(new_openb_bytes, 1);
	if (!new_openb) {
		kfree(new_table);
		return -1;
	}

	memset(new_table, 0, new_cap * sizeof(handle_t));
	memset(new_openb, 0, new_openb_bytes);

	if (fdt->table && fdt->cap) {
		memcpy(new_table, fdt->table, fdt->cap * sizeof(handle_t));
	}
	
	if (fdt->openb) {
		memcpy(new_openb, fdt->openb, (fdt->cap + 7) / 8);
	}

	kfree(fdt->table);
	kfree(fdt->openb);

	fdt->table = new_table;
	fdt->openb = new_openb;
	fdt->cap = new_cap;

	return 0;
}

int handle_alloc(task_t* t, inode_type_t type) {
	if (!t) {
		return PANUTIERRNO_PLAINERR;
	}

	fdtable_t* fdt = &t->fdtable;

	for (uint32_t byte = 0; byte < (fdt->cap + 7) / 8; byte++) {
		if (fdt->openb[byte] == 0xFF) {
			continue;
		}

		for (uint32_t bit = 0; bit < 8; bit++) {
			uint32_t slot = byte * 8 + bit;
			if (slot >= fdt->cap) {
				break;
			}
			
			if (fdt->openb[byte] & (1u << bit)) {
				continue;
			}

			fdt->openb[byte] |= (1u << bit);
			fdt->table[slot].type = type;
			return (int)slot;
		}
	}

	uint32_t slot = fdt->cap;
	if (fdtable_grow(t) != 0) {
		return PANUTIERRNO_PLAINERR;
	}

	fdt->openb[slot / 8] |= (1u << (slot % 8));
	fdt->table[slot].type = type;

	return (int)slot;
}

void handle_free(task_t* t, int fd) {
	if (!t || fd < 0) {
		return;
	}

	fdtable_t* fdt = &t->fdtable;
	if (!fdt->table || !fdt->openb || (uint32_t)fd >= fdt->cap) {
		return;
	}

	uint32_t slot = (uint32_t)fd;
	fdt->openb[slot / 8] &= ~(1u << (slot % 8));

	fdt->table[slot].type = INODE_NONE;
	fdt->table[slot].impl = nullptr;
	fdt->table[slot].ops = nullptr;
	fdt->table[slot].inode = nullptr;

	uint32_t openb_bytes = (fdt->cap + 7) / 8;
	for (uint32_t byte = 0; byte < openb_bytes; byte++) {
		if (fdt->openb[byte]) {
			return;
		}
	}

	kfree(fdt->table);
	kfree(fdt->openb);
	fdt->table = nullptr;
	fdt->openb = nullptr;
	fdt->cap = 0;
}

bool handle_build(const char* path, handle_t* out) {
	inode_t* n = registry_resolve(registry_root(), path);
	if (!n) {
		return false;
	}

	out->type = n->type;
	out->impl = n->impl;
	out->ops = n->ops;
	out->inode = n;

	inode_ref(n);
	return true;
}