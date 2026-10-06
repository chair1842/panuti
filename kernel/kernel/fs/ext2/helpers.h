/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef KERNEL_FS_EXT2_HELPERS_H
#define KERNEL_FS_EXT2_HELPERS_H

#include <kernel/fs/ext2.h>
#include <stdint.h>

#define EXT2_DIRENT_NO_SPACE (-2)

static inline uint32_t div_ceil_u32(uint32_t x, uint32_t y) {
    return (uint32_t)(((uint64_t)x + y - 1) / y);
}

enum {
	EXT2_DIRENT_FULL = 0,    // no room in this block, try another one
	EXT2_DIRENT_PLACED = 1,  // the block now holds the record and must be written back
	EXT2_DIRENT_EXISTS = 2,  // the name is already present
	EXT2_DIRENT_CORRUPT = 3, // the block is malformed past this point
};

uint32_t ext2_sectors_per_block(const ext2_t* fs);
int ext2_read_block(ext2_t* fs, uint32_t block, void* buf);
int ext2_write_block(ext2_t* fs, uint32_t block, const void* buf);
uint64_t ext2_inode_size(ext2_t* fs, const ext2_inode_hdr_t* hdr);
int ext2_read_inode(ext2_t* fs, uint32_t inum, ext2_inode_hdr_t* out);
void ext2_inode_cache_store(ext2_t* fs, uint32_t inum, const ext2_inode_hdr_t* hdr);
int ext2_inode_locate(ext2_t* fs, uint32_t inum, uint32_t* table_block, uint32_t* byte_off);
int ext2_write_inode(ext2_t* fs, uint32_t inum, const ext2_inode_hdr_t* hdr);
int ext2_indirect_entry(ext2_t* fs, uint32_t block, uint32_t index, uint32_t* out);
int ext2_indirect_peek(ext2_t* fs, uint32_t block, uint32_t index, uint32_t* out);
int ext2_indirect_store(ext2_t* fs, uint32_t block, uint32_t index, uint32_t value);
int ext2_zero_block(ext2_t* fs, uint32_t block);
int ext2_map_block(ext2_t* fs, const uint32_t* i_block, uint32_t index, uint32_t* out);
int ext2_map_block_alloc(ext2_t* fs, ext2_inode_hdr_t* hdr, uint32_t index, uint32_t* out);
int ext2_block_slot(uint32_t index, uint32_t apb, uint32_t* root, uint32_t* depth, uint32_t* slots);
int ext2_free_index(ext2_t* fs, ext2_inode_hdr_t* hdr, uint32_t index, uint32_t* freed);
int ext2_free_blocks_from(ext2_t* fs, ext2_inode_hdr_t* hdr, uint32_t first_index, uint32_t last_index, uint32_t* freed);

static inline uint32_t ext2_dirent_hdr_len(const ext2_t* fs) {
	(void)fs;
	return (uint32_t)sizeof(ext2_dirent_hdr_t);
}

uint32_t ext2_dirent_rec_len(const ext2_t* fs, size_t name_len);
int ext2_dirent_name_ok(const char* name, size_t name_len);
int ext2_dirent_at(const ext2_t* fs, const uint8_t* buf, uint32_t off, ext2_dirent_hdr_t* out);
int ext2_dirent_find(ext2_t* fs, const ext2_inode_hdr_t* dir, const char* name, size_t name_len, uint32_t* inum);
int ext2_dirent_is_empty(ext2_t* fs, const ext2_inode_hdr_t* dir, bool* empty);
int ext2_inode_retire(ext2_t* fs, uint32_t inum);
int ext2_dirs_count_adjust(ext2_t* fs, uint32_t inum, int delta);
int ext2_dirent_add(ext2_t* fs, ext2_inode_hdr_t* dir, uint32_t dir_inum, uint32_t inum, const char* name, size_t name_len, uint8_t file_type);
int ext2_dirent_init_dir(const ext2_t* fs, uint8_t* buf, uint32_t inum, uint32_t parent);
int ext2_dirent_remove(ext2_t* fs, ext2_inode_hdr_t* dir, const char* name, size_t name_len);
int ext2_dirent_set_parent(ext2_t* fs, ext2_inode_hdr_t* dir, uint32_t parent_inum);
int ext2_bitmap_sync(ext2_t* fs, ext2_bitmap_cache_t* bc);
int ext2_sync_metadata(ext2_t* fs);
int ext2_bitmap_get(ext2_t* fs, ext2_bitmap_cache_t* bc, uint32_t bitmap_block, uint8_t** out);
uint32_t ext2_group_blocks(ext2_t* fs, uint32_t group);
uint32_t ext2_first_allocatable(ext2_t* fs);
int ext2_alloc_block(ext2_t* fs, uint32_t* out);
int ext2_free_block(ext2_t* fs, uint32_t block);
int ext2_alloc_inode(ext2_t* fs, uint32_t* out);
int ext2_free_inode(ext2_t* fs, uint32_t inum);

#endif
