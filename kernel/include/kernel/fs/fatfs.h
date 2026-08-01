#ifndef _KERNEL_FS_FATFS_H
#define _KERNEL_FS_FATFS_H

#include <kernel/block/block.h>
#include <stdint.h>

typedef enum fatfs_type {
	FATFSTY_FAT32,
	FATFSTY_FAT16,
	FATFSTY_FAT12,
} fatfs_type_t;

typedef struct fatfs {
	fatfs_type_t type;

	block_dev_t* blk_dev;

	// bp in here means "bytes per"
	uint32_t bp_sector;
	uint32_t bp_cluster;

	uint32_t reserved_sectors; // no of reserved secs before FAT
	
	uint32_t fat_count;
	
	uint32_t fat_start_sec;
	uint32_t fat_size; // in sectors

	uint32_t data_start_sec;

	uint32_t total_clusters;

	// FAT32-spec
	uint32_t root_cluster;

	// FAT16/12-spec
	uint32_t root_dir_start_sec;
	uint32_t root_dir_size;
	
	uint8_t* fat_cache;
} fatfs_t;

// Mounts a FAT filesystem from the given block device path to the given mount point.
// both paths must exist already and be valid.
int fatfs_mount(const char* mountp, const char* blkdev_path);

#endif