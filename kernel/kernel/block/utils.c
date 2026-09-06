#include "kernel/block/block.h"
#include "kernel/memman/slab.h"
#include <kernel/block/utils.h>
#include <panuti/errno.h>
#include <string.h>

int block_read_bytes(block_dev_t *dev, uint64_t offset, uint32_t len, void *buf) {
	uint32_t blks = dev->block_size;
	uint64_t startblk = offset / blks;
	uint64_t endblk = (offset + len - 1) / blks;
	uint32_t blkc = (uint32_t)(endblk - startblk + 1);

	if (endblk >= dev->block_count) {
		return BLOCK_ERR_INVAL;
	}

	// the stars are aligned. you may now kiss your crush.
	if (offset % blks == 0 && len % blks == 0) {
		return dev->ops->read(dev->impl, startblk, buf, len / blks);
	}

	// the stars aren't aligned. you will take glances but never appreach her.
	void* scratch = kmalloc((size_t)blkc * blks, blks);
	if (!scratch) {
		return BLOCK_ERR_IO;
	}

	int rc = dev->ops->read(dev->impl, startblk, scratch, blkc);
	if (rc == BLOCK_OK) {
		uint32_t skip = (uint32_t)(offset - startblk * blks);
		memcpy(buf, (uint8_t*)scratch + skip, len);
	}

	kfree(scratch);
	return rc;
}