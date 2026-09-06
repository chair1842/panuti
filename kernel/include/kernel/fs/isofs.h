#ifndef _KERNEL_FS_ISOFS_H
#define _KERNEL_FS_ISOFS_H
// isofs is the filesystem implementation for iso9660 or lesser known, ecma 119

#include "kernel/handle/registry.h"
#include <kernel/block/block.h>
#include <stdbool.h>

#define ISOFS_BLOCKSIZE 2048

typedef struct isofs_dirent {
	uint32_t start_lba;
	uint32_t length;
	bool is_dir;
	char name[REG_MAX_NAME_LEN];
} isofs_dirent_t;

typedef struct isofs {
	block_dev_t* block_device;
	
	uint32_t block_size;
	uint32_t volume_space_size;

	isofs_dirent_t root;
} isofs_t;

typedef struct isofs_file {
	isofs_t* fs;
	uint32_t start_lba;
	uint32_t length;
} isofs_file_t;

int isofs_mount(const char* mountp, const char* blkdev);

#endif