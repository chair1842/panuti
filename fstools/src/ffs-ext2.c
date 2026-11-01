/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <panuti/handle.h>
#include <panuti/stat.h>
#include <panuti/inode_type.h>
#include <panuti/seek.h>

#define EXT2_MAGIC 0xEF53
#define EXT2_INCOMPAT_FILETYPE 0x0002
#define EXT2_RO_COMPAT_SPARSE_SUPER 0x0001
#define EXT2_RO_COMPAT_LARGE_FILE 0x0002
#define EXT2_STATE_CLEAN 1
#define EXT2_ROOT_INO 2

#define FS_BLOCK_SIZE 1024
#define FS_FIRST_DATA_BLOCK 1
#define FS_INODE_SIZE 128
#define FS_FIRST_INODE 11
#define FS_INODES_PER_GROUP 1024
#define FS_BLOCKS_COUNT 512
#define FS_INODES_COUNT 1024
#define FS_GROUP_COUNT 1
#define FS_USED_BLOCKS 6
#define FS_USED_INODES 1

typedef struct __attribute__((packed)) {
	uint32_t inodes_count;
	uint32_t blocks_count;
	uint32_t root_blocks_count;
	uint32_t free_blocks_count;
	uint32_t free_inodes_count;
	uint32_t first_data_block;
	uint32_t log_block_size;
	uint32_t log_frag_size;
	uint32_t blocks_per_group;
	uint32_t frags_per_group;
	uint32_t inodes_per_group;
	uint32_t mtime;
	uint32_t wtime;
	uint16_t mnt_count;
	int16_t max_mnt_count;
	uint16_t magic;
	uint16_t state;
	uint16_t errors;
	uint16_t minor_rev_level;
	uint32_t lastcheck;
	uint32_t checkinterval;
	uint32_t creator_os;
	uint32_t rev_level;
	uint16_t def_resuid;
	uint16_t def_resgid;
	uint32_t first_inode;
	uint16_t inode_size;
	uint16_t block_group_nr;
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
	uint8_t reserved[816];
} ext2_superblock_t;

typedef struct __attribute__((packed)) {
	uint32_t block_bitmap;
	uint32_t inode_bitmap;
	uint32_t inode_table;
	uint16_t free_blocks_count;
	uint16_t free_inodes_count;
	uint16_t used_dirs_count;
	uint16_t pad;
	uint8_t reserved[12];
} ext2_group_desc_t;

typedef struct __attribute__((packed)) {
	uint16_t mode;
	uint16_t uid;
	uint32_t size;
	uint32_t atime;
	uint32_t ctime;
	uint32_t mtime;
	uint32_t dtime;
	uint16_t gid;
	uint16_t links_count;
	uint32_t blocks;
	uint32_t flags;
	uint32_t osd1;
	uint32_t block[15];
	uint32_t generation;
	uint32_t file_acl;
	uint32_t size_high;
} ext2_inode_hdr_t;

typedef struct __attribute__((packed)) {
	uint32_t inode;
	uint16_t rec_len;
	uint8_t name_len;
	uint8_t file_type;
} ext2_dirent_hdr_t;

static const char HELP[] =
	"ffs-ext2 - a fstools utility\n\n"
	"ffs-ext2 formats a block device to ext2\n\n"
	"usage:\n"
	"  ffs-ext2 <blkdev>\n\n"
	"args:\n"
	"  <blkdev> - block device to format, such as /dvc/pata/0 or /dvc/ram/0\n"
	"  -h - prints this help message\n";

static int want_help(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			return 1;
		}
	}

	return 0;
}

int main(int argc, char** argv) {
	if (want_help(argc, argv)) {
		printf("%s", HELP);
		return 0;
	}

	if (argc < 2) {
		printf("fstools: ffs-ext2: missing block device\n");
		printf("%s", HELP);
		return -1;
	}

	const char* path = argv[1];
	if (path[0] == '-' && path[1] != '\0') {
		printf("fstools: ffs-ext2: invalid option '%s'\n", path + 1);
		return -1;
	}

	dirent_entry_t st;
	if (stat(path, &st) != 0) {
		printf("fstools: ffs-ext2: %s: cannot stat block device\n", path);
		return -1;
	}
	if (st.type != INODE_BLOCK) {
		printf("fstools: ffs-ext2: %s: not a block device\n", path);
		return -1;
	}

	int fd = handle_open(path);
	if (fd < 0) {
		printf("fstools: ffs-ext2: %s: cannot open block device\n", path);
		return -1;
	}

	unsigned char* img = (unsigned char*)malloc((size_t)FS_BLOCKS_COUNT * FS_BLOCK_SIZE);
	if (!img) {
		handle_close(fd);
		printf("fstools: ffs-ext2: out of memory\n");
		return -1;
	}

	memset(img, 0, (size_t)FS_BLOCKS_COUNT * FS_BLOCK_SIZE);

	ext2_superblock_t sb;
	memset(&sb, 0, sizeof(sb));
	sb.inodes_count = FS_INODES_COUNT;
	sb.blocks_count = FS_BLOCKS_COUNT;
	sb.root_blocks_count = 1;
	sb.free_blocks_count = FS_BLOCKS_COUNT - FS_USED_BLOCKS;
	sb.free_inodes_count = FS_INODES_COUNT - FS_USED_INODES;
	sb.first_data_block = FS_FIRST_DATA_BLOCK;
	sb.log_block_size = 0;
	sb.log_frag_size = 0;
	sb.blocks_per_group = 8192;
	sb.frags_per_group = 8192;
	sb.inodes_per_group = FS_INODES_PER_GROUP;
	sb.magic = EXT2_MAGIC;
	sb.state = EXT2_STATE_CLEAN;
	sb.errors = 1;
	sb.rev_level = 1;
	sb.first_inode = FS_FIRST_INODE;
	sb.inode_size = FS_INODE_SIZE;
	sb.block_group_nr = 0;
	sb.feature_incompat = EXT2_INCOMPAT_FILETYPE;
	sb.feature_ro_compat = EXT2_RO_COMPAT_SPARSE_SUPER | EXT2_RO_COMPAT_LARGE_FILE;
	memcpy(img + 1024, &sb, sizeof(sb));

	ext2_group_desc_t gd;
	memset(&gd, 0, sizeof(gd));
	gd.block_bitmap = 3;
	gd.inode_bitmap = 4;
	gd.inode_table = 5;
	gd.free_blocks_count = (uint16_t)(FS_BLOCKS_COUNT - FS_USED_BLOCKS);
	gd.free_inodes_count = (uint16_t)(FS_INODES_COUNT - FS_USED_INODES);
	gd.used_dirs_count = 1;
	memcpy(img + 2048, &gd, sizeof(gd));

	ext2_inode_hdr_t inode;
	memset(&inode, 0, sizeof(inode));
	inode.mode = 0x4000;
	inode.uid = 0;
	inode.gid = 0;
	inode.size = FS_BLOCK_SIZE;
	inode.links_count = 2;
	inode.blocks = 2;
	inode.block[0] = 133;
	memcpy(img + 5 * FS_BLOCK_SIZE + (EXT2_ROOT_INO - 1) * FS_INODE_SIZE, &inode, sizeof(inode));

	ext2_dirent_hdr_t dot;
	memset(&dot, 0, sizeof(dot));
	dot.inode = EXT2_ROOT_INO;
	dot.rec_len = 12;
	dot.name_len = 1;
	dot.file_type = 2;
	memcpy(img + 133 * FS_BLOCK_SIZE, &dot, sizeof(dot));

	ext2_dirent_hdr_t dotdot;
	memset(&dotdot, 0, sizeof(dotdot));
	dotdot.inode = EXT2_ROOT_INO;
	dotdot.rec_len = FS_BLOCK_SIZE - 12;
	dotdot.name_len = 2;
	dotdot.file_type = 2;
	memcpy(img + 133 * FS_BLOCK_SIZE + 12, &dotdot, sizeof(dotdot));

	for (uint32_t b = 1; b <= FS_BLOCKS_COUNT; b++) {
		if (b == 1 || b == 2 || b == 3 || b == 4 || b == 5 || b == 133) {
			img[3 * FS_BLOCK_SIZE + ((b - 1) / 8)] |= (unsigned char)(1u << ((b - 1) % 8));
		}
	}

	img[4 * FS_BLOCK_SIZE] |= 0x02;

	int w = handle_write(fd, img, (int)((size_t)FS_BLOCKS_COUNT * FS_BLOCK_SIZE));
	handle_close(fd);
	free(img);

	if (w != (int)((size_t)FS_BLOCKS_COUNT * FS_BLOCK_SIZE)) {
		printf("fstools: ffs-ext2: write to %s failed\n", path);
		return -1;
	}

	printf("fstools: ffs-ext2: formatted %s\n", path);
	return 0;
}
