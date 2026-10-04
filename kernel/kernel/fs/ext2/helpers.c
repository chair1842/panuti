/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "helpers.h"
#include <kernel/block/block.h>
#include <kernel/block/utils.h>
#include <kernel/memman/slab.h>
#include <panuti/errno.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

uint32_t ext2_sectors_per_block(const ext2_t* fs) {
	return fs->block_size / 512;
}

int ext2_read_block(ext2_t* fs, uint32_t block, void* buf) {
    uint64_t offset = (uint64_t)block * fs->block_size;

    if (offset > UINT64_MAX - fs->block_size) {
        return BLOCK_ERR_INVAL;
    }

    if (block >= fs->superblock->blocks_count) {
        return BLOCK_ERR_INVAL;
    }

    return block_read_bytes(fs->block_device, offset, fs->block_size, buf);
}

int ext2_write_block(ext2_t* fs, uint32_t block, const void* buf) {
    uint64_t offset = (uint64_t)block * fs->block_size;

    if (offset > UINT64_MAX - fs->block_size) {
        return BLOCK_ERR_INVAL;
    }

    if (block >= fs->superblock->blocks_count) {
        return BLOCK_ERR_INVAL;
    }

    return block_write_bytes(fs->block_device, offset, fs->block_size, buf);
}

uint64_t ext2_inode_size(ext2_t* fs, const ext2_inode_hdr_t* hdr) {
	if (fs->large_files && hdr->size_high != 0) {
		return ((uint64_t)hdr->size_high << 32) | hdr->size;
	}

	return hdr->size;
}

int ext2_read_inode(ext2_t* fs, uint32_t inum, ext2_inode_hdr_t* out) {
	if (inum == 0 || inum > fs->superblock->inodes_count) {
		return -1;
	}

	// A directory scan names a long run of neighbouring inodes, and lookup asks
	// for the same ones again and again, so a handful of slots removes most of
	// those reads. Safe to keep for the whole mount: nothing here is writable.
	for (uint32_t i = 0; i < EXT2_INODE_CACHE_SLOTS; i++) {
		if (fs->inode_cache[i].num == inum) {
			memcpy(out, &fs->inode_cache[i].hdr, sizeof(*out));
			return 0;
		}
	}

	uint32_t table_block = 0;
	uint32_t byte_off = 0;

	if (ext2_inode_locate(fs, inum, &table_block, &byte_off) != 0) {
		return -1;
	}

	uint64_t offset = (uint64_t)table_block * fs->block_size + byte_off;

	if (offset > UINT64_MAX - sizeof(ext2_inode_hdr_t)) {
		return -1;
	}

	ext2_inode_cache_slot_t* slot = &fs->inode_cache[fs->inode_cache_next];

	fs->inode_cache_next = (fs->inode_cache_next + 1) % EXT2_INODE_CACHE_SLOTS;
	slot->num = 0; // only becomes a hit once the read below actually succeeds

	if (block_read_bytes(fs->block_device, offset, sizeof(ext2_inode_hdr_t), &slot->hdr) != BLOCK_OK) {
		return -1;
	}

	slot->num = inum;
	memcpy(out, &slot->hdr, sizeof(*out));

	return 0;
}

void ext2_inode_cache_store(ext2_t* fs, uint32_t inum, const ext2_inode_hdr_t* hdr) {
	for (uint32_t i = 0; i < EXT2_INODE_CACHE_SLOTS; i++) {
		if (fs->inode_cache[i].num == inum) {
			memcpy(&fs->inode_cache[i].hdr, hdr, sizeof(*hdr));
			return;
		}
	}

	ext2_inode_cache_slot_t* slot = &fs->inode_cache[fs->inode_cache_next];

	fs->inode_cache_next = (fs->inode_cache_next + 1) % EXT2_INODE_CACHE_SLOTS;
	memcpy(&slot->hdr, hdr, sizeof(*hdr));
	slot->num = inum;
}

int ext2_inode_locate(ext2_t* fs, uint32_t inum, uint32_t* table_block, uint32_t* byte_off) {
	if (inum == 0 || inum > fs->superblock->inodes_count) {
		return -1;
	}

	uint32_t group = (inum - 1) / fs->superblock->inodes_per_group;

	if (group >= fs->group_count) {
		return -1;
	}

	uint32_t index_in_group = (inum - 1) % fs->superblock->inodes_per_group;

	*table_block = fs->gdt[group].inode_table + index_in_group / fs->inodes_per_block;
	*byte_off = (index_in_group % fs->inodes_per_block) * fs->inode_size;

	if (*table_block >= fs->superblock->blocks_count) {
		return -1;
	}

	if (*byte_off > fs->block_size || fs->block_size - *byte_off < sizeof(ext2_inode_hdr_t)) {
		return -1;
	}

	return 0;
}

int ext2_write_inode(ext2_t* fs, uint32_t inum, const ext2_inode_hdr_t* hdr) {
	uint32_t table_block = 0;
	uint32_t byte_off = 0;

	if (ext2_inode_locate(fs, inum, &table_block, &byte_off) != 0) {
		return BLOCK_ERR_INVAL;
	}

	uint64_t offset = (uint64_t)table_block * fs->block_size + byte_off;

	if (offset > UINT64_MAX - sizeof(ext2_inode_hdr_t)) {
		return BLOCK_ERR_INVAL;
	}

	int rc = block_write_bytes(fs->block_device, offset, sizeof(ext2_inode_hdr_t), hdr);

	if (rc == BLOCK_OK) {
		ext2_inode_cache_store(fs, inum, hdr);
	}

	return rc;
}

int ext2_indirect_entry(ext2_t* fs, uint32_t block, uint32_t index, uint32_t* out) {
	if (block == 0 || index >= fs->addrs_per_block) {
		return -1;
	}

	if (block >= fs->superblock->blocks_count) {
		return -1;
	}

	if (fs->cached_indirect_block != block) {
		if (!fs->cached_indirect_buf) {
			return -1;
		}

		if (ext2_read_block(fs, block, fs->cached_indirect_buf) != BLOCK_OK) {
			fs->cached_indirect_block = 0;
			return -1;
		}

		fs->cached_indirect_block = block;
	}

	// the address is unaligned as often as not
	uint32_t value;

	memcpy(&value, fs->cached_indirect_buf + (uint64_t)index * sizeof(uint32_t), sizeof(value));

	if (value == 0 || value >= fs->superblock->blocks_count) {
		return -1;
	}

	*out = value;

	return 0;
}

int ext2_indirect_peek(ext2_t* fs, uint32_t block, uint32_t index, uint32_t* out) {
	if (block == 0 || index >= fs->addrs_per_block) {
		return -1;
	}

	if (block >= fs->superblock->blocks_count) {
		return -1;
	}

	uint8_t* buf = kmalloc(fs->block_size, fs->block_size);

	if (!buf) {
		return -1;
	}

	uint32_t value = 0;

	if (ext2_read_block(fs, block, buf) != BLOCK_OK) {
		kfree(buf);
		return -1;
	}

	memcpy(&value, buf + (uint64_t)index * sizeof(uint32_t), sizeof(value));
	kfree(buf);

	*out = value;

	return 0;
}

int ext2_indirect_store(ext2_t* fs, uint32_t block, uint32_t index, uint32_t value) {
	if (block == 0 || index >= fs->addrs_per_block) {
		return -1;
	}

	if (block >= fs->superblock->blocks_count) {
		return -1;
	}

	uint8_t* buf = kmalloc(fs->block_size, fs->block_size);

	if (!buf) {
		return -1;
	}

	int rc = -1;

	if (ext2_read_block(fs, block, buf) == BLOCK_OK) {
		memcpy(buf + (uint64_t)index * sizeof(uint32_t), &value, sizeof(value));

		if (ext2_write_block(fs, block, buf) == BLOCK_OK) {
			if (fs->cached_indirect_block == block) {
				fs->cached_indirect_block = 0;
			}

			rc = 0;
		}
	}

	kfree(buf);

	return rc;
}

int ext2_zero_block(ext2_t* fs, uint32_t block) {
	uint8_t* buf = kmalloc(fs->block_size, fs->block_size);

	if (!buf) {
		return -1;
	}

	memset(buf, 0, fs->block_size);

	int rc = ext2_write_block(fs, block, buf);

	kfree(buf);

	return rc == BLOCK_OK ? 0 : -1;
}

int ext2_map_block(ext2_t* fs, const uint32_t* i_block, uint32_t index, uint32_t* out) {
	uint32_t apb = fs->addrs_per_block;

	if (index < EXT2_NDIR_BLOCKS) {
		if (i_block[index] == 0 || i_block[index] >= fs->superblock->blocks_count) {
			return -1;
		}

		*out = i_block[index];

		return 0;
	}

	index -= EXT2_NDIR_BLOCKS;

	if (index < apb) {
		return ext2_indirect_entry(fs, i_block[EXT2_IND_BLOCK], index, out);
	}

	index -= apb;

	// apb tops out at 1024, so squaring it cannot overflow
	uint64_t dind_span = (uint64_t)apb * apb;

	if (index < dind_span) {
		uint32_t ind;

		if (ext2_indirect_entry(fs, i_block[EXT2_DIND_BLOCK], index / apb, &ind) != 0) {
			return -1;
		}

		return ext2_indirect_entry(fs, ind, index % apb, out);
	}

	index -= dind_span;

	uint32_t dind, ind;

	if (ext2_indirect_entry(fs, i_block[EXT2_TIND_BLOCK], index / dind_span, &dind) != 0) {
		return -1;
	}

	if (ext2_indirect_entry(fs, dind, (index / apb) % apb, &ind) != 0) {
		return -1;
	}

	return ext2_indirect_entry(fs, ind, index % apb, out);
}

int ext2_map_block_alloc(ext2_t* fs, ext2_inode_hdr_t* hdr, uint32_t index, uint32_t* out) {
	uint32_t apb = fs->addrs_per_block;
	uint32_t spb = ext2_sectors_per_block(fs);

	// at most a root plus one block per level, so the rollback list is fixed
	uint32_t taken[4];
	uint32_t taken_count = 0;

	if (index < EXT2_NDIR_BLOCKS) {
		if (hdr->block[index] == 0) {
			uint32_t nb = 0;

			if (ext2_alloc_block(fs, &nb) != 0) {
				return -1;
			}

			taken[taken_count++] = nb;
			hdr->block[index] = nb;
			hdr->blocks += spb;
		}

		*out = hdr->block[index];
		return 0;
	}

	index -= EXT2_NDIR_BLOCKS;

	uint32_t root = EXT2_IND_BLOCK;
	uint32_t depth = 1;
	uint32_t slots[3];

	if (index < apb) {
		slots[0] = index;
	} else {
		index -= apb;

		uint32_t dind_span = apb * apb;

		if (index < dind_span) {
			root = EXT2_DIND_BLOCK;
			depth = 2;
			
			slots[0] = index / apb;
			slots[1] = index % apb;
		} else {
			index -= dind_span;
			root = EXT2_TIND_BLOCK;
			depth = 3;
			
			slots[0] = index / dind_span;
			slots[1] = (index / apb) % apb;
			slots[2] = index % apb;
		}
	}

	uint32_t container = hdr->block[root];

	if (container == 0) {
		uint32_t nb = 0;

		if (ext2_alloc_block(fs, &nb) != 0) {
			return -1;
		}

		taken[taken_count++] = nb;
		container = nb;
		
		hdr->block[root] = nb;
		hdr->blocks += spb;
	}

	for (uint32_t d = 0; d < depth; d++) {
		uint32_t entry = 0;

		if (ext2_indirect_peek(fs, container, slots[d], &entry) != 0) {
			goto fail;
		}

		if (entry == 0) {
			uint32_t nb = 0;

			if (ext2_alloc_block(fs, &nb) != 0) {
				goto fail;
			}

			if (ext2_indirect_store(fs, container, slots[d], nb) != 0) {
				ext2_free_block(fs, nb);
				goto fail;
			}

			taken[taken_count++] = nb;
			hdr->blocks += spb;
			entry = nb;
		}

		container = entry;
	}

	*out = container;
	return 0;

fail:
	hdr->blocks -= taken_count * spb;

	while (taken_count > 0) {
		ext2_free_block(fs, taken[--taken_count]);
	}

	return -1;
}

int ext2_bitmap_sync(ext2_t* fs, ext2_bitmap_cache_t* bc) {
	if (!bc->dirty) {
		return BLOCK_OK;
	}

	if (!bc->buf || bc->cached == 0) {
		return BLOCK_ERR_INVAL;
	}

	int rc = ext2_write_block(fs, bc->cached - 1, bc->buf);

	bc->dirty = false;

	return rc;
}

int ext2_bitmap_get(ext2_t* fs, ext2_bitmap_cache_t* bc, uint32_t bitmap_block, uint8_t** out) {
	if (!bc->buf || bitmap_block == 0 || bitmap_block >= fs->superblock->blocks_count) {
		return -1;
	}

	if (bc->cached == bitmap_block + 1) {
		*out = bc->buf;
		return 0;
	}

	if (ext2_bitmap_sync(fs, bc) != BLOCK_OK) {
		return -1;
	}

	if (ext2_read_block(fs, bitmap_block, bc->buf) != BLOCK_OK) {
		bc->cached = 0;
		return -1;
	}

	bc->cached = bitmap_block + 1;
	bc->dirty = false;
	*out = bc->buf;

	return 0;
}

uint32_t ext2_group_blocks(ext2_t* fs, uint32_t group) {
	uint32_t bpg = fs->superblock->blocks_per_group;
	uint64_t start = (uint64_t)group * bpg + fs->first_data_block;

	if (start >= fs->superblock->blocks_count) {
		return 0;
	}

	uint64_t left = fs->superblock->blocks_count - start;

	return left > bpg ? bpg : (uint32_t)left;
}

uint32_t ext2_first_allocatable(ext2_t* fs) {
	uint32_t itables = div_ceil_u32(fs->inode_size * fs->superblock->inodes_per_group, fs->block_size);

	return fs->gdt[0].inode_table + itables;
}

int ext2_alloc_block(ext2_t* fs, uint32_t* out) {
	uint32_t bpg = fs->superblock->blocks_per_group;
	uint32_t bit_cap = fs->block_size * 8;

	for (uint32_t n = 0; n < fs->group_count; n++) {
		uint32_t group = (fs->block_alloc_hint + n) % fs->group_count;
		uint32_t avail = ext2_group_blocks(fs, group);
		uint8_t* bm = nullptr;

		if (avail == 0) {
			continue;
		}

		if (ext2_bitmap_get(fs, &fs->block_bitmap, fs->gdt[group].block_bitmap, &bm) != 0) {
			return -1;
		}

		if (avail > bit_cap) {
			avail = bit_cap;
		}

		for (uint32_t i = 0; i < avail; i++) {
			uint32_t block = group * bpg + fs->first_data_block + i;

			if (group == 0 && block < ext2_first_allocatable(fs)) {
				continue;
			}

			uint8_t byte = bm[i >> 3];

			if (byte & (1u << (i & 7))) {
				continue;
			}

			bm[i >> 3] = byte | (1u << (i & 7));
			fs->block_bitmap.dirty = true;

			if (ext2_zero_block(fs, block) != 0) {
				bm[i >> 3] = byte;
				fs->block_bitmap.dirty = true;
				return -1;
			}

			if (fs->superblock->free_blocks_count > 0) {
				fs->superblock->free_blocks_count--;
			}

			fs->superblock_dirty = true;

			if (fs->gdt[group].free_blocks_count > 0) {
                fs->gdt[group].free_blocks_count--;
            }

			fs->gdt_dirty = true;

			fs->block_alloc_hint = group;

			*out = block;
			return 0;
		}
	}

	return -1;
}

int ext2_free_block(ext2_t* fs, uint32_t block) {
	uint32_t bpg = fs->superblock->blocks_per_group;

	if (block < fs->first_data_block) {
		return -1;
	}

	uint32_t rel = block - fs->first_data_block;
	uint32_t group = rel / bpg;
	uint32_t i = rel % bpg;
	uint32_t bit_cap = fs->block_size * 8;
	uint8_t* bm = nullptr;

	if (group >= fs->group_count || i >= bit_cap || i >= ext2_group_blocks(fs, group)) {
		return -1;
	}

	if (group == 0 && block < ext2_first_allocatable(fs)) {
		return -1;
	}

	if (ext2_bitmap_get(fs, &fs->block_bitmap, fs->gdt[group].block_bitmap, &bm) != 0) {
		return -1;
	}

	if (!(bm[i >> 3] & (1u << (i & 7)))) {
		return -1;
	}

	bm[i >> 3] &= (uint8_t)~(1u << (i & 7));
	fs->block_bitmap.dirty = true;

	fs->superblock->free_blocks_count++;
	fs->superblock_dirty = true;
	fs->gdt[group].free_blocks_count++;
	fs->gdt_dirty = true;

	return 0;
}

int ext2_alloc_inode(ext2_t* fs, uint32_t* out) {
	uint32_t ipg = fs->superblock->inodes_per_group;
	uint32_t bit_cap = fs->block_size * 8;

	for (uint32_t n = 0; n < fs->group_count; n++) {
		uint32_t group = (fs->inode_alloc_hint + n) % fs->group_count;
		uint8_t* bm = nullptr;

		if (ext2_bitmap_get(fs, &fs->inode_bitmap, fs->gdt[group].inode_bitmap, &bm) != 0) {
			return -1;
		}

		// the last group holds fewer inodes than a full one
		uint64_t first_inum = (uint64_t)group * ipg + 1;
		uint64_t left = fs->superblock->inodes_count + 1 - first_inum;

		if (first_inum > fs->superblock->inodes_count) {
			continue;
		}

		uint32_t avail = left > ipg ? ipg : (uint32_t)left;

		if (avail > bit_cap) {
			avail = bit_cap;
		}

		for (uint32_t i = 0; i < avail; i++) {
			uint32_t inum = group * ipg + i + 1;

			// everything below s_first_inode is reserved by the format
			if (inum < fs->first_inode) {
				continue;
			}

			uint8_t byte = bm[i >> 3];

			if (byte & (1u << (i & 7))) {
				continue;
			}

			bm[i >> 3] = byte | (1u << (i & 7));
			fs->inode_bitmap.dirty = true;

			if (fs->superblock->free_inodes_count > 0) {
				fs->superblock->free_inodes_count--;
			}

			fs->superblock_dirty = true;

			if (fs->gdt[group].free_inodes_count > 0) {
				fs->gdt[group].free_inodes_count--;
			}

			fs->gdt_dirty = true;
			fs->inode_alloc_hint = group;

			*out = inum;
			return 0;
		}
	}

	return -1;
}

int ext2_free_inode(ext2_t* fs, uint32_t inum) {
	uint32_t ipg = fs->superblock->inodes_per_group;

	if (inum < fs->first_inode || inum > fs->superblock->inodes_count) {
		return -1;
	}

	uint32_t group = (inum - 1) / ipg;

	if (group >= fs->group_count) {
		return -1;
	}

	uint32_t i = (inum - 1) % ipg;
	uint8_t* bm = nullptr;

	if (i >= fs->block_size * 8) {
		return -1;
	}

	if (ext2_bitmap_get(fs, &fs->inode_bitmap, fs->gdt[group].inode_bitmap, &bm) != 0) {
		return -1;
	}

	if (!(bm[i >> 3] & (1u << (i & 7)))) {
		return -1;
	}

	bm[i >> 3] &= (uint8_t)~(1u << (i & 7));
	fs->inode_bitmap.dirty = true;

	fs->superblock->free_inodes_count++;
	fs->superblock_dirty = true;
	fs->gdt[group].free_inodes_count++;
	fs->gdt_dirty = true;

	return 0;
}
