/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "kernel/block/block.h"
#include <kernel/fs/ext2.h>
#include <kernel/handle/registry.h>
#include <panuti/errno.h>
#include <kernel/memman/slab.h>
#include <kernel/block/utils.h>
#include <stdint.h>

#define EXT2_MAGIC 0xEF53

#define EXT2_INCOMPAT_FILETYPE 0x0002
#define EXT2_INCOMPAT_SUPPORTED (EXT2_INCOMPAT_FILETYPE)

#define EXT2_RO_COMPAT_SPARSE_SUPER 0x0001
#define EXT2_RO_COMPAT_LARGE_FILE 0x0002
#define EXT2_RO_COMPAT_SUPPORTED (EXT2_RO_COMPAT_SPARSE_SUPER | EXT2_RO_COMPAT_LARGE_FILE)

#define EXT2_STATE_CLEAN 1

#define EXT2_WRITES_IMPLEMENTED 0

#define EXT2_MAX_INODE_SIZE 256

static inline uint32_t div_ceil_u32(uint32_t x, uint32_t y) {
    return (uint32_t)(((uint64_t)x + y - 1) / y);
}

static int ext2_read_block(ext2_t* fs, uint32_t block, void* buf) {
    uint64_t offset = (uint64_t)block * fs->block_size;

    if (offset > UINT64_MAX - fs->block_size) {
        return BLOCK_ERR_INVAL;
    }

    if (block >= fs->superblock->blocks_count) {
        return BLOCK_ERR_INVAL;
    }

    return block_read_bytes(fs->block_device, offset, fs->block_size, buf);
}

static int ext2_validate_gdt(ext2_t* fs) {
    uint32_t itables_in_group = div_ceil_u32(fs->inode_size * fs->superblock->inodes_per_group, fs->block_size);

    for (uint32_t i = 0; i < fs->group_count; i++) {
        const ext2_group_desc_t* gd = &fs->gdt[i];

        if (gd->block_bitmap >= fs->superblock->blocks_count ||
            gd->inode_bitmap >= fs->superblock->blocks_count) {
            return -1;
        }

        if (gd->inode_table >= fs->superblock->blocks_count) {
            return -1;
        }

        // the inode table of the last group runs to the end of the volume
        if ((uint64_t)gd->inode_table + itables_in_group > fs->superblock->blocks_count) {
            return -1;
        }
    }

    return 0;
}

int ext2_mount(const char* mountp, const char* blkdev) {
	block_dev_t* dev = block_find(blkdev);
	if (!dev) {
		return PANUTIERRNO_NOTFOUND;
	}

	// the smallest thing that can hold a superblock and one data block
	if (dev->block_count * dev->block_size < 2048) {
		return PANUTIERRNO_PLAINERR;
	}

	ext2_t* fs = kmalloc(sizeof(ext2_t), alignof(ext2_t));
	if (!fs) {
		return PANUTIERRNO_PLAINERR;
	}

	ext2_superblock_t* sb = kmalloc(sizeof(ext2_superblock_t), alignof(ext2_superblock_t));
	if (!sb) {
		kfree(fs);
		return PANUTIERRNO_PLAINERR;
	}

	if (block_read_bytes(dev, 1024, sizeof(ext2_superblock_t), sb) != BLOCK_OK) {
		goto fail;
	}

	fs->superblock = sb;
	fs->block_device = dev;

	if (sb->magic != EXT2_MAGIC) {
		goto fail;
	}

	// there are only 0 and 1
	if (sb->rev_level > 1) {
		goto fail;
	}

	if (sb->log_block_size > 2) {
		goto fail;
	}

	fs->block_size = 1024 << sb->log_block_size;

	if (sb->blocks_count == 0 || sb->inodes_count == 0) {
		goto fail;
	}

	if (sb->blocks_per_group == 0 || sb->inodes_per_group == 0) {
		goto fail;
	}

	if (sb->blocks_per_group > 8 * fs->block_size) {
		goto fail;
	}

	if (sb->inodes_per_group > 8 * fs->block_size) {
		goto fail;
	}

	if (fs->block_size == 1024) {
		if (sb->first_data_block != 1) {
			goto fail;
		}
	} else {
		if (sb->first_data_block != 0) {
			goto fail;
		}
	}

	fs->first_data_block = sb->first_data_block;

	if (
		((uint64_t)sb->blocks_count * fs->block_size)
		>
	    ((uint64_t)dev->block_size * dev->block_count)
	) {
		goto fail;
	}

	fs->group_count = div_ceil_u32(sb->blocks_count - sb->first_data_block, sb->blocks_per_group);

	if (fs->group_count == 0) {
		goto fail;
	}

	// group_count * inodes_per_group overflows 32 bits well before a large
	// volume does, so this has to be widened too
	if ((uint64_t)sb->inodes_count > (uint64_t)fs->group_count * sb->inodes_per_group) {
		goto fail;
	}

	if (sb->rev_level == 0) {
		fs->inode_size = 128;
		fs->first_inode = 11;
		fs->incompat = 0;
		fs->ro_compat = 0;
	} else {
		uint32_t insz = sb->inode_size;

		if (insz < 128 || (insz & (insz - 1)) != 0 ||
		    insz > fs->block_size || insz > EXT2_MAX_INODE_SIZE) {
			goto fail;
		}

		if (sb->first_inode < 11 || sb->first_inode > sb->inodes_count) {
			goto fail;
		}

		fs->inode_size = insz;
		fs->first_inode = sb->first_inode;
		fs->incompat = sb->feature_incompat;
		fs->ro_compat = sb->feature_ro_compat;
	}

	if (fs->incompat & ~EXT2_INCOMPAT_SUPPORTED) {
		goto fail;
	}

	fs->has_filetype = (fs->incompat & EXT2_INCOMPAT_FILETYPE) != 0;
	fs->large_files = (fs->ro_compat & EXT2_RO_COMPAT_LARGE_FILE) != 0;

	fs->addrs_per_block = fs->block_size / 4;
	fs->inodes_per_block = fs->block_size / fs->inode_size;

	uint64_t gdt_bytes = (uint64_t)fs->group_count * sizeof(ext2_group_desc_t);
	uint64_t gdt_blocks = (gdt_bytes + fs->block_size - 1) / fs->block_size;

	if (gdt_blocks == 0 || gdt_blocks > UINT32_MAX) {
		goto fail;
	}

	// the table has to end inside the volume
	if ((uint64_t)fs->first_data_block + 1 + gdt_blocks > sb->blocks_count) {
		goto fail;
	}

	uint64_t gdt_alloc = gdt_blocks * fs->block_size;
	if (gdt_alloc == 0 || gdt_alloc > UINT32_MAX) {
		goto fail;
	}

	fs->gdt_blocks = (uint32_t)gdt_blocks;

	fs->gdt = kmalloc((uint32_t)gdt_alloc, fs->block_size);
	if (!fs->gdt) {
		goto fail;
	}

	for (uint32_t i = 0; i < fs->gdt_blocks; i++) {
		if (
			ext2_read_block(fs, fs->first_data_block + 1 + i,
			(uint8_t*)fs->gdt + (uint64_t)i * fs->block_size) != BLOCK_OK
		) {
			goto fail;
		}
	}

	if (ext2_validate_gdt(fs) != 0) {
		goto fail;
	}

	bool read_only = (fs->ro_compat & ~EXT2_RO_COMPAT_SUPPORTED) != 0 || sb->state != EXT2_STATE_CLEAN;

	fs->read_only = !EXT2_WRITES_IMPLEMENTED || read_only;

	inode_t* mountpoint = registry_resolve(registry_root(), mountp);
	if (!mountpoint || mountpoint->type != INODE_DIR) {
		goto fail;
	}

	return PANUTIERRNO_PLAINSUCCESS;

fail:
	if (fs->gdt) {
		kfree(fs->gdt);
	}
	
	kfree(sb);
	kfree(fs);
	return PANUTIERRNO_PLAINERR;
}
