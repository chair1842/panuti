/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/pardet.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <kernel/block/block.h>
#include <kernel/memman/slab.h>
#include <kernel/klog.h>

#define MBR_SIZE 512
#define MBR_SIGNATURE 0xAA55
#define MBR_TABLE_OFFSET 0x1BE
#define MBR_ENTRY_SIZE 16
#define MBR_MAX_PARTITIONS 4

#define MBR_TYPE_EMPTY 0x00
#define MBR_TYPE_GPT_PROTECTIVE 0xEE

typedef struct {
	block_dev_t* parent;
	uint64_t start_lba;
	uint64_t block_count;
} pardet_partition_t;

static inline uint32_t read_le32(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3]);
}

static int part_read(void* impl, uint64_t block, void* buf, size_t count) {
	pardet_partition_t* part = (pardet_partition_t*)impl;
	if (block + count > part->block_count) {
		return BLOCK_ERR_INVAL;
	}
	return part->parent->ops->read(part->parent->impl, part->start_lba + block, buf, count);
}

static int part_write(void* impl, uint64_t block, const void* buf, size_t count) {
	pardet_partition_t* part = (pardet_partition_t*)impl;
	if (block + count > part->block_count) {
		return BLOCK_ERR_INVAL;
	}
	return part->parent->ops->write(part->parent->impl, part->start_lba + block, buf, count);
}

static uint64_t part_count(void* impl) {
	return ((pardet_partition_t*)impl)->block_count;
}

static const block_ops_t part_ops = {
	.read = part_read,
	.write = part_write,
	.count = part_count,
};

int pardet_create_partitions(const char* blkdev) {
	if (!blkdev) {
		return -1;
	}

	block_dev_t* disk = block_find(blkdev);
	if (!disk || !disk->ops || !disk->ops->read) {
		return -1;
	}

	uint8_t mbr[MBR_SIZE];
	if (disk->ops->read(disk->impl, 0, mbr, 1) != BLOCK_OK) {
		return -1;
	}

	uint16_t signature = (uint16_t)mbr[510] | ((uint16_t)mbr[511] << 8);
	if (signature != MBR_SIGNATURE) {
		return 0; // no partition table, could be a superfloppy
	}

	size_t dlen = strlen(blkdev);
	int registered = 0;

	for (int i = 0; i < MBR_MAX_PARTITIONS; i++) {
		const uint8_t* entry = mbr + MBR_TABLE_OFFSET + i * MBR_ENTRY_SIZE;

		uint8_t status = entry[0];
		uint8_t type = entry[4];
		uint32_t start = read_le32(entry + 8);
		uint32_t count = read_le32(entry + 12);

		if (type == MBR_TYPE_EMPTY || type == MBR_TYPE_GPT_PROTECTIVE) {
			continue;
		}

		// 0x00 for a plain primary, 0x80 for a bootable one
		if (status != 0x00 && status != 0x80) {
			continue;
		}

		if (count == 0) {
			continue;
		}

		if ((uint64_t)start + count > disk->block_count) {
			klog(KLOG_WARN, "pardet: %s p%d claims sectors %u..%u, past the end of the disk\n",
					blkdev, i + 1, start, (uint32_t)((uint64_t)start + count));
			continue;
		}

		pardet_partition_t* part = kmalloc(sizeof(pardet_partition_t), alignof(pardet_partition_t));
		if (!part) {
			klog(KLOG_WARN, "pardet: kmalloc failed for %s p%d\n", blkdev, i + 1);
			break;
		}

		part->parent = disk;
		part->start_lba = start;
		part->block_count = count;

		char path[24];
		if (dlen + 3 >= sizeof(path)) {
			kfree(part);
			break;
		}

		memcpy(path, blkdev, dlen);
		path[dlen] = 'p';
		path[dlen + 1] = (char)('1' + i);
		path[dlen + 2] = '\0';

		if (!block_register(path, &part_ops, part, disk->block_size, count)) {
			kfree(part);
			break;
		}

		klog(KLOG_INFO, "pardet: %s type 0x%02x, %u sectors at lba %u\n", path, type, count, start);
		registered++;
	}

	return registered;
}