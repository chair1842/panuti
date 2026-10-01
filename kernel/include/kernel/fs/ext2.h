/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_FS_EXT2_H
#define _KERNEL_FS_EXT2_H

#include <kernel/block/block.h>

typedef struct __attribute__((packed)) ext2_superblock {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t root_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block; // 1 for 1K blocks, else 0
    uint32_t log_block_size; // block_size = 1024 << this
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime; // last mount time (unix seconds)
    uint32_t wtime; // last write time
    uint16_t mnt_count; // mounts since last fsck
    int16_t  max_mnt_count; // fsck after this many (-1 = disabled)
    uint16_t magic; // must be 0xEF53
    uint16_t state; // 1 = clean, 2 = has errors
    uint16_t errors; // what to do on error: 1 continue, 2 remount ro, 3 panic
    uint16_t minor_rev_level;
    uint32_t lastcheck; // last fsck time
    uint32_t checkinterval; // max time between fscks
    uint32_t creator_os;
    uint32_t rev_level; // 0 = old fixed layout, 1 = dynamic
    uint16_t def_resuid; // default uid for reserved blocks
    uint16_t def_resgid;

    // everything below is only valid if s_rev_level >= 1
    uint32_t first_inode; // first non-reserved inode (rev 0: 11)
    uint16_t inode_size; // bytes per inode (rev 0: 128)
    uint16_t block_group_nr; // which group this superblock copy is in
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    char volume_name[16];
    char last_mounted[64];
    uint32_t algo_bitmap;
    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t padding1;

    uint8_t reserved[816]; // journal fields etc. (ext3/4); pad to 1024
} ext2_superblock_t;

typedef struct __attribute__((packed)) ext2_group_desc {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint8_t reserved[12];
} ext2_group_desc_t;

typedef struct ext2 {
	block_dev_t* block_device;
	
	ext2_superblock_t* superblock;

	uint32_t block_size;
	uint32_t first_data_block;
	uint32_t group_count;
	uint32_t inode_size;
	uint32_t first_inode;
	uint32_t inodes_per_block;
	uint32_t addrs_per_block;
	uint32_t gdt_blocks;

	ext2_group_desc_t* gdt;

	uint32_t incompat;
	uint32_t ro_compat;

	bool has_filetype;
	bool large_files;

	bool read_only;

	bool superblock_dirty;
	bool gdt_dirty;

	uint8_t* scratch;

	uint32_t cached_indirect_block;
	uint8_t* cached_indirect_buf;
} ext2_t;

int ext2_mount(const char* mountp, const char* blkdev);

#endif