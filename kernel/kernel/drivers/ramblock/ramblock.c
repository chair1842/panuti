#include <kernel/block/block.h>
#include <kernel/memman/slab.h>
#include <stdalign.h>
#include <string.h>
#include "ramblock.h"

typedef struct {
	uint8_t* buffer;
	uint32_t block_size;
	uint64_t block_count;
} ramblock_t;

static int ramblock_read(void* impl, uint64_t block, void* buf, size_t count) {
	ramblock_t* rb = (ramblock_t*)impl;
	if (block + count > rb->block_count) {
		return BLOCK_ERR_INVAL;
	}
	
	memcpy(buf, rb->buffer + block * rb->block_size, count * rb->block_size);
	return BLOCK_OK;
}

static int ramblock_write(void* impl, uint64_t block, const void* buf, size_t count) {
	ramblock_t* rb = (ramblock_t*)impl;
	if (block + count > rb->block_count) {
		return BLOCK_ERR_INVAL;
	}
	
	memcpy(rb->buffer + block * rb->block_size, buf, count * rb->block_size);
	return BLOCK_OK;
}

static uint64_t ramblock_count(void* impl) {
	ramblock_t* rb = (ramblock_t*)impl;
	return rb->block_count;
}

static const block_ops_t ramblock_ops = {
	.read = ramblock_read,
	.write = ramblock_write,
	.count = ramblock_count,
};

void ramblock_init(const char* path, uint32_t block_size, uint64_t block_count) {
	ramblock_t* rb = kmalloc(sizeof(ramblock_t), alignof(ramblock_t));
	if (!rb) {
		return;
	}

	rb->buffer = kmalloc((size_t)block_size * (size_t)block_count, 1);
	if (!rb->buffer) {
		kfree(rb);
		return;
	}

	memset(rb->buffer, 0, (size_t)block_size * (size_t)block_count);
	rb->block_size = block_size;
	rb->block_count = block_count;

	block_register(path, &ramblock_ops, rb, block_size, block_count);
}
