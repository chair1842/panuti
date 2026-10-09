/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "helpers.h"
#include <kernel/block/block.h>
#include <kernel/block/utils.h>
#include <kernel/memman/slab.h>
#include <panuti/errno.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

// the free path is reached from ext2_map_block_alloc's rollback, which sits
// above it in the file
static int ext2_free_slot(ext2_t* fs, ext2_inode_hdr_t* hdr, uint32_t index, uint32_t* freed);

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

int ext2_block_slot(uint32_t index, uint32_t apb, uint32_t* root, uint32_t* depth, uint32_t* slots) {
	if (index < EXT2_NDIR_BLOCKS) {
		return -1;
	}

	index -= EXT2_NDIR_BLOCKS;

	if (index < apb) {
		*root = EXT2_IND_BLOCK;
		*depth = 1;
		slots[0] = index;

		return 0;
	}

	index -= apb;

	// apb tops out at 1024, so squaring it cannot overflow 32 bits
	uint32_t dind_span = apb * apb;

	if (index < dind_span) {
		*root = EXT2_DIND_BLOCK;
		*depth = 2;

		slots[0] = index / apb;
		slots[1] = index % apb;

		return 0;
	}

	index -= dind_span;

	*root = EXT2_TIND_BLOCK;
	*depth = 3;

	slots[0] = index / dind_span;
	slots[1] = (index / apb) % apb;
	slots[2] = index % apb;

	return 0;
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

	uint32_t root = 0, depth = 0, slots[3] = {0};

	if (ext2_block_slot(index, apb, &root, &depth, slots) != 0) {
		return -1;
	}

	uint32_t container = i_block[root];

	for (uint32_t d = 0; d < depth; d++) {
		uint32_t entry = 0;

		if (ext2_indirect_entry(fs, container, slots[d], &entry) != 0) {
			return -1;
		}

		container = entry;
	}

	*out = container;

	return 0;
}

int ext2_map_block_alloc(ext2_t* fs, ext2_inode_hdr_t* hdr, uint32_t index, uint32_t* out) {
	uint32_t apb = fs->addrs_per_block;
	uint32_t spb = ext2_sectors_per_block(fs);

	if (index < EXT2_NDIR_BLOCKS) {
		if (hdr->block[index] == 0) {
			uint32_t nb = 0;

			if (ext2_alloc_block(fs, &nb) != 0) {
				return -1;
			}

			hdr->block[index] = nb;
			hdr->blocks += spb;
		}

		*out = hdr->block[index];
		return 0;
	}

	uint32_t root = 0, depth = 0, slots[3] = {0};

	if (ext2_block_slot(index, apb, &root, &depth, slots) != 0) {
		return -1;
	}

	uint32_t container = hdr->block[root];

	if (container == 0) {
		uint32_t nb = 0;

		if (ext2_alloc_block(fs, &nb) != 0) {
			return -1;
		}

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

			hdr->blocks += spb;
			entry = nb;
		}

		container = entry;
	}

	*out = container;
	return 0;

fail:
	uint32_t freed = 0;

	if (ext2_free_slot(fs, hdr, index, &freed) == 0) {
		hdr->blocks -= freed * spb;
	}

	return -1;
}

static int ext2_block_empty(ext2_t* fs, uint32_t block, int* empty) {
	uint8_t* buf = kmalloc(fs->block_size, fs->block_size);

	if (!buf) {
		return -1;
	}

	int rc = 0;

	if (ext2_read_block(fs, block, buf) != BLOCK_OK) {
		rc = -1;
		goto out;
	}

	*empty = 1;

	// only the address slots matter, and padding past them stays zero
	uint64_t span = (uint64_t)fs->addrs_per_block * sizeof(uint32_t);

	for (uint64_t i = 0; i < span; i++) {
		if (buf[i] != 0) {
			*empty = 0;
			break;
		}
	}

out:
	kfree(buf);
	return rc;
}

static int ext2_free_slot(ext2_t* fs, ext2_inode_hdr_t* hdr, uint32_t index, uint32_t* freed) {
	*freed = 0;

	if (index >= fs->superblock->blocks_count) {
		return -1;
	}

	if (index < EXT2_NDIR_BLOCKS) {
		uint32_t block = hdr->block[index];

		if (block == 0 || block >= fs->superblock->blocks_count) {
			return 0; // already free
		}

		if (ext2_free_block(fs, block) != 0) {
			return -1;
		}

		hdr->block[index] = 0;
		*freed = 1;

		return 0;
	}

	uint32_t root = 0, depth = 0, slots[3] = {0};

	if (ext2_block_slot(index, fs->addrs_per_block, &root, &depth, slots) != 0) {
		return -1;
	}

	uint32_t containers[3];
	uint32_t reached = 0;
	uint32_t container = hdr->block[root];
	int walked = 0;

	for (uint32_t d = 0; d < depth; d++) {
		if (container == 0 || container >= fs->superblock->blocks_count) {
			goto prune; // unmapped above: nothing below to release
		}

		containers[d] = container;
		reached = d + 1;

		uint32_t entry = 0;

		if (ext2_indirect_peek(fs, container, slots[d], &entry) != 0) {
			return -1;
		}

		if (entry == 0) {
			goto prune; // hole below: prune the containers we did reach
		}

		container = entry;
	}

	walked = 1;

prune:
	if (walked && container != 0 && container < fs->superblock->blocks_count) {
		if (ext2_free_block(fs, container) != 0) {
			return -1;
		}

		*freed = 1;

		if (ext2_indirect_store(fs, containers[depth - 1], slots[depth - 1], 0) != 0) {
			return -1;
		}
	}

	for (uint32_t d = reached; d-- > 0;) {
		int empty = 0;

		if (ext2_block_empty(fs, containers[d], &empty) != 0) {
			return -1;
		}

		if (!empty) {
			break;
		}

		if (ext2_free_block(fs, containers[d]) != 0) {
			return -1;
		}

		*freed += 1;

		if (d == 0) {
			hdr->block[root] = 0;
		} else if (ext2_indirect_store(fs, containers[d - 1], slots[d - 1], 0) != 0) {
			return -1;
		}
	}

	return 0;
}

int ext2_free_index(ext2_t* fs, ext2_inode_hdr_t* hdr, uint32_t index, uint32_t* freed) {
	if (!fs || !hdr) {
		return -1;
	}

	return ext2_free_slot(fs, hdr, index, freed);
}

static int ext2_free_level(ext2_t* fs, uint32_t container, uint32_t base, uint32_t stride,
                           uint32_t depth, uint32_t first_index, uint32_t last_index,
                           uint32_t* freed, int* emptied) {
	uint8_t* buf = kmalloc(fs->block_size, fs->block_size);

	if (!buf) {
		return -1;
	}

	if (ext2_read_block(fs, container, buf) != BLOCK_OK) {
		kfree(buf);
		return -1;
	}

	int changed = 0;

	for (uint32_t k = 0; k < fs->addrs_per_block; k++) {
		uint64_t idx = (uint64_t)base + (uint64_t)k * stride;

		if (idx > last_index) {
			break;
		}

		uint32_t entry = 0;
		memcpy(&entry, buf + (uint64_t)k * sizeof(uint32_t), sizeof(entry));

		if (entry == 0) {
			continue;
		}

		if (depth == 0) {
			if (idx < first_index) {
				continue;   // inside the part that survives
			}

			if (ext2_free_block(fs, entry) != 0) {
				kfree(buf);
				return -1;
			}

			memset(buf + (uint64_t)k * sizeof(uint32_t), 0, sizeof(entry));
			changed = 1;
			(*freed)++;
			continue;
		}

		uint32_t child_stride = (depth == 2) ? fs->addrs_per_block : 1;
		int child_empty = 0;

		if (ext2_free_level(fs, entry, (uint32_t)idx, child_stride, depth - 1,
		                    first_index, last_index, freed, &child_empty) != 0) {
			kfree(buf);
			return -1;
		}

		if (child_empty) {
			if (ext2_free_block(fs, entry) != 0) {
				kfree(buf);
				return -1;
			}

			memset(buf + (uint64_t)k * sizeof(uint32_t), 0, sizeof(entry));
			changed = 1;
			(*freed)++;
		}
	}

	if (changed && ext2_write_block(fs, container, buf) != BLOCK_OK) {
		kfree(buf);
		return -1;
	}

	*emptied = 1;

	for (uint32_t k = 0; k < fs->addrs_per_block; k++) {
		uint32_t entry = 0;
		memcpy(&entry, buf + (uint64_t)k * sizeof(uint32_t), sizeof(entry));

		if (entry != 0) {
			*emptied = 0;
			break;
		}
	}

	kfree(buf);

	return 0;
}

int ext2_free_blocks_from(ext2_t* fs, ext2_inode_hdr_t* hdr, uint32_t first_index, uint32_t last_index, uint32_t* freed) {
	if (!fs || !hdr) {
		return -1;
	}

	*freed = 0;

	if (first_index > last_index) {
		return 0;
	}

	// direct slots first
	for (uint32_t i = first_index; i < EXT2_NDIR_BLOCKS && i <= last_index; i++) {
		uint32_t block = hdr->block[i];

		if (block == 0 || block >= fs->superblock->blocks_count) {
			continue;
		}

		if (ext2_free_block(fs, block) != 0) {
			return -1;
		}

		hdr->block[i] = 0;
		(*freed)++;
	}

	uint32_t apb = fs->addrs_per_block;

	// one row per level: which i_block slot roots it, where its slots start in
	// logical block numbers, how far apart they sit, and how deep it goes
	const struct {
		uint32_t root;
		uint32_t base;
		uint32_t stride;
		uint32_t depth;
	} levels[3] = {
		{EXT2_IND_BLOCK,  EXT2_NDIR_BLOCKS, 1u, 0u},
		{EXT2_DIND_BLOCK, EXT2_NDIR_BLOCKS + apb, apb, 1u},
		{EXT2_TIND_BLOCK, EXT2_NDIR_BLOCKS + apb + apb * apb, apb * apb, 2u},
	};

	for (int level = 0; level < 3; level++) {
		uint32_t container = hdr->block[levels[level].root];

		if (container == 0 || container >= fs->superblock->blocks_count) {
			continue;
		}

		int emptied = 0;

		if (ext2_free_level(fs, container, levels[level].base, levels[level].stride,
		                    levels[level].depth, first_index, last_index,
		                    freed, &emptied) != 0) {
			return -1;
		}

		if (emptied) {
			if (ext2_free_block(fs, container) != 0) {
				return -1;
			}

			hdr->block[levels[level].root] = 0;
			(*freed)++;
		}
	}

	return 0;
}

int ext2_bitmap_sync(ext2_t* fs, ext2_bitmap_cache_t* bc) {
	if (!bc->dirty) {
		return BLOCK_OK;
	}

	if (!bc->buf || bc->cached == 0) {
		return BLOCK_ERR_INVAL;
	}

	int rc = ext2_write_block(fs, bc->cached - 1, bc->buf);

	if (rc == BLOCK_OK) {
		bc->dirty = false;
	}

	return rc;
}

uint32_t ext2_dirent_rec_len(const ext2_t* fs, size_t name_len) {
	return (ext2_dirent_hdr_len(fs) + (uint32_t)name_len + 3u) & ~3u;
}

static int ext2_all_zero(const uint8_t* p, uint32_t len) {
	for (uint32_t i = 0; i < len; i++) {
		if (p[i] != 0) {
			return 0;
		}
	}

	return 1;
}

int ext2_dirent_at(const ext2_t* fs, const uint8_t* buf, uint32_t off, ext2_dirent_hdr_t* out) {
	uint32_t hdr_len = ext2_dirent_hdr_len(fs);

	if (off > fs->block_size - hdr_len) {
		return 0;
	}

	memset(out, 0, sizeof(*out));
	memcpy(out, buf + off, hdr_len);

	if (out->rec_len == 0) {
		return 0; // the rest of this block is unused
	}

	if (out->rec_len < hdr_len ||
		(out->rec_len % 4) != 0 ||
		out->rec_len > fs->block_size ||
		off > fs->block_size - out->rec_len) {
		return -1;
	}

	return 1;
}

static void ext2_dirent_write(const ext2_t* fs, uint8_t* buf, uint32_t off,
	uint32_t inum, const char* name, size_t name_len, uint8_t file_type, uint32_t rec_len) {

	uint32_t hdr_len = ext2_dirent_hdr_len(fs);

	memset(buf + off, 0, rec_len);

	ext2_dirent_hdr_t h;

	memset(&h, 0, sizeof(h));

	h.inode = inum;
	h.rec_len = (uint16_t)rec_len;
	h.name_len = (uint8_t)name_len;
	h.file_type = fs->has_filetype ? file_type : 0;

	memcpy(buf + off, &h, hdr_len);
	memcpy(buf + off + hdr_len, name, name_len);
}

int ext2_dirent_init_dir(const ext2_t* fs, uint8_t* buf, uint32_t inum, uint32_t parent) {
	if (!fs || !buf || inum == 0) {
		return -1;
	}

	uint32_t self_len = ext2_dirent_rec_len(fs, 1);
	uint32_t up_len = ext2_dirent_rec_len(fs, 2);

	if (fs->block_size < self_len + up_len + ext2_dirent_hdr_len(fs)) {
		return -1;
	}

	memset(buf, 0, fs->block_size);

	ext2_dirent_write(fs, buf, 0, inum, ".", 1, EXT2_FT_DIR, self_len);
	ext2_dirent_write(fs, buf, self_len, parent, "..", 2, EXT2_FT_DIR, up_len);

	uint32_t rest = fs->block_size - self_len - up_len;

	ext2_dirent_hdr_t free_rec;

	memset(&free_rec, 0, sizeof(free_rec));
	free_rec.rec_len = (uint16_t)rest;

	memcpy(buf + self_len + up_len, &free_rec, ext2_dirent_hdr_len(fs));

	return 0;
}

static int ext2_dirent_place(const ext2_t* fs, uint8_t* buf, uint32_t need,
	const char* name, size_t name_len, uint32_t inum, uint8_t file_type) {

	uint32_t hdr_len = ext2_dirent_hdr_len(fs);
	uint32_t off = 0;
	uint32_t split_off = UINT32_MAX;
	uint32_t hole_off = UINT32_MAX;
	uint32_t hole_len = 0;
	uint32_t last_off = 0;
	uint32_t last_len = 0;
	int seen = 0;

	for (;;) {
		ext2_dirent_hdr_t d;
		int st = ext2_dirent_at(fs, buf, off, &d);

		if (st < 0) {
			return EXT2_DIRENT_CORRUPT;
		}

		if (st == 0) {
			break;
		}

		// a deleted record has no name worth validating
		if (d.inode != 0) {
			if (d.name_len == 0 || d.name_len > d.rec_len - hdr_len) {
				return EXT2_DIRENT_CORRUPT;
			}

			if (d.name_len == name_len &&
				memcmp(buf + off + hdr_len, name, name_len) == 0) {
				return EXT2_DIRENT_EXISTS;
			}
		}

		if (d.inode == 0) {
			if (d.rec_len >= need && d.rec_len > hole_len) {
				hole_len = d.rec_len;
				hole_off = off;
			}
		} else if (split_off == UINT32_MAX && d.rec_len > need) {
			if (d.rec_len - need >= ext2_dirent_rec_len(fs, d.name_len)) {
				split_off = off;
			}
		}

		last_off = off;
		last_len = d.rec_len;
		seen = 1;

		off += d.rec_len;
	}

	// an existing hole needs no surgery at all
	if (hole_len >= need) {
		ext2_dirent_write(fs, buf, hole_off, inum, name, name_len, file_type, hole_len);
		return EXT2_DIRENT_PLACED;
	}

	// otherwise carve the tail off a record that is longer than we need
	if (split_off != UINT32_MAX) {
		ext2_dirent_hdr_t d;
		uint32_t keep;
		uint32_t self;

		ext2_dirent_at(fs, buf, split_off, &d);

		keep = d.rec_len - need;
		self = ext2_dirent_rec_len(fs, d.name_len);

		d.rec_len = (uint16_t)keep;
		memcpy(buf + split_off, &d, hdr_len);
		memset(buf + split_off + self, 0, keep - self);

		ext2_dirent_write(fs, buf, split_off + keep, inum, name, name_len, file_type, need);
		return EXT2_DIRENT_PLACED;
	}

	if (!seen) {
		// a wholly empty block is all free space
		ext2_dirent_write(fs, buf, 0, inum, name, name_len, file_type, need);
		return EXT2_DIRENT_PLACED;
	}

	uint32_t tail_off = last_off + last_len;

	if (fs->block_size - tail_off >= need &&
		ext2_all_zero(buf + tail_off, fs->block_size - tail_off)) {
		ext2_dirent_write(fs, buf, tail_off, inum, name, name_len, file_type, need);
		return EXT2_DIRENT_PLACED;
	}

	return EXT2_DIRENT_FULL;
}

int ext2_dirent_add(ext2_t* fs, ext2_inode_hdr_t* dir, uint32_t dir_inum,
	uint32_t inum, const char* name, size_t name_len, uint8_t file_type) {

	if (!fs || !dir || !name || inum == 0) {
		return -1;
	}

	if ((dir->mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
		return -1;
	}

	if (!ext2_dirent_name_ok(name, name_len)) {
		return -1;
	}

	// ext2_readdir caches this directory's size, block array and one of its
	// dirent blocks under cached_dir_inode; anything written below can make
	// that snapshot miss entries, so drop it before mutating
	if (fs->cached_dir_inode == dir_inum) {
		fs->cached_dir_inode = 0;
	}

	uint32_t need = ext2_dirent_rec_len(fs, name_len);

	if (need > fs->block_size) {
		return -1;
	}

	uint32_t i_block[15];

	memcpy(i_block, dir->block, sizeof(i_block));

	uint32_t dsize = dir->size;

	if ((uint64_t)dsize > (uint64_t)fs->superblock->blocks_count * fs->block_size) {
		return -1;
	}

	uint32_t nblocks = (dsize + fs->block_size - 1) / fs->block_size;
	uint8_t* buf = kmalloc(fs->block_size, fs->block_size);

	if (!buf) {
		return -1;
	}

	int rc = -1;
	int stop = 0;
	int verdict = 0;

	for (uint32_t bi = 0; bi < nblocks && !stop; bi++) {
		uint32_t phys = 0;
		int st;

		if (ext2_map_block(fs, i_block, bi, &phys) != 0 || phys == 0) {
			continue; // sparse directory, a later block may still have room
		}

		if (ext2_read_block(fs, phys, buf) != BLOCK_OK) {
			continue;
		}

		st = ext2_dirent_place(fs, buf, need, name, name_len, inum, file_type);

		if (st == EXT2_DIRENT_EXISTS) {
			rc = EXT2_DIRENT_EXISTS;
			verdict = 1;
			stop = 1;
		} else if (st == EXT2_DIRENT_CORRUPT) {
			rc = -1;
			verdict = 1;
			stop = 1;
		} else if (st == EXT2_DIRENT_PLACED) {
			rc = (ext2_write_block(fs, phys, buf) == BLOCK_OK) ? 0 : -1;
			verdict = 1;
			stop = 1;
		}
	}

	if (rc == -1 && !stop) {
		uint32_t phys = 0;
		uint32_t old_blocks = dir->blocks;

		if (ext2_map_block(fs, i_block, nblocks, &phys) == 0 && phys != 0) {
			rc = -1;
		} else if (ext2_map_block_alloc(fs, dir, nblocks, &phys) != 0) {
			rc = -1;
		} else {
			ext2_dirent_write(fs, buf, 0, inum, name, name_len, file_type, fs->block_size);

			if (ext2_write_block(fs, phys, buf) == BLOCK_OK) {
				dir->size = (nblocks + 1) * fs->block_size;

				if (ext2_write_inode(fs, dir_inum, dir) != 0) {
					uint32_t freed = 0;

					ext2_free_index(fs, dir, nblocks, &freed);
					dir->size = dsize;
					dir->blocks = old_blocks;
				} else {
					rc = 0;
				}
			} else {
				uint32_t freed = 0;

				ext2_free_index(fs, dir, nblocks, &freed);
			}
		}
	}

	kfree(buf);

	// a name that is already taken is a different answer from one that simply
	// will not fit, and a caller that has to act on the difference cannot guess.
	// every other failure keeps the plain -1, so callers testing != 0 are
	// unaffected
	return rc == -1 && !verdict ? EXT2_DIRENT_NO_SPACE : rc;
}

int ext2_dirent_remove(ext2_t* fs, ext2_inode_hdr_t* dir, const char* name, size_t name_len) {
	uint32_t hdr_len;
	uint32_t dsize;
	uint32_t nblocks;
	uint32_t i_block[15];
	uint8_t* buf;
	int rc = -1;

	if (!fs || !dir || !name) {
		return -1;
	}

	if ((dir->mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
		return -1;
	}

	if (!ext2_dirent_name_ok(name, name_len)) {
		return -1;
	}

	// same snapshot as above; this variant does not know which directory it
	// is editing, so drop whatever is cached and let the next readdir reload
	fs->cached_dir_inode = 0;

	hdr_len = ext2_dirent_hdr_len(fs);
	dsize = dir->size;

	memcpy(i_block, dir->block, sizeof(i_block));

	if ((uint64_t)dsize > (uint64_t)fs->superblock->blocks_count * fs->block_size) {
		return -1;
	}

	nblocks = (dsize + fs->block_size - 1) / fs->block_size;
	buf = kmalloc(fs->block_size, fs->block_size);

	if (!buf) {
		return -1;
	}

	for (uint32_t bi = 0; bi < nblocks; bi++) {
		uint32_t phys = 0;
		uint32_t off = 0;

		if (ext2_map_block(fs, i_block, bi, &phys) != 0 || phys == 0) {
			continue;
		}

		if (ext2_read_block(fs, phys, buf) != BLOCK_OK) {
			continue;
		}

		for (;;) {
			ext2_dirent_hdr_t d;
			ext2_dirent_hdr_t nx;
			uint32_t total;
			uint32_t next;
			int st = ext2_dirent_at(fs, buf, off, &d);

			if (st <= 0) {
				break; // padding or an untrustworthy record, move on
			}

			if (d.inode != 0 &&
				(d.name_len == 0 || d.name_len > d.rec_len - hdr_len)) {
				break;
			}

			if (d.inode != 0 &&
				d.name_len == name_len &&
				memcmp(buf + off + hdr_len, name, name_len) == 0) {

				total = d.rec_len;
				next = off + total;

				if (ext2_dirent_at(fs, buf, next, &nx) == 1 && nx.inode == 0) {
					total += nx.rec_len;
					memset(buf + next, 0, nx.rec_len); // rec_len 0 ends this block's list
				}

				d.inode = 0;
				d.name_len = 0;
				d.file_type = 0;
				d.rec_len = (uint16_t)total;

				memcpy(buf + off, &d, hdr_len);
				memset(buf + off + hdr_len, 0, total - hdr_len);

				rc = (ext2_write_block(fs, phys, buf) == BLOCK_OK) ? 0 : -1;
				goto done;
			}

			off += d.rec_len;
		}
	}

done:
	kfree(buf);

	return rc;
}

int ext2_dirent_set_parent(ext2_t* fs, ext2_inode_hdr_t* dir, uint32_t parent_inum) {
	uint32_t hdr_len;
	uint32_t nblocks;
	uint32_t i_block[15];
	uint8_t* buf;
	int rc = -1;

	if (!fs || !dir) {
		return -1;
	}

	if ((dir->mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
		return -1;
	}

	if (parent_inum == 0) {
		return -1;
	}

	if ((uint64_t)dir->size > (uint64_t)fs->superblock->blocks_count * fs->block_size) {
		return -1;
	}

	hdr_len = ext2_dirent_hdr_len(fs);
	nblocks = div_ceil_u32(dir->size, fs->block_size);
	memcpy(i_block, dir->block, sizeof(i_block));

	buf = kmalloc(fs->block_size, fs->block_size);

	if (!buf) {
		return -1;
	}

	for (uint32_t bi = 0; bi < nblocks; bi++) {
		uint32_t phys = 0;
		uint32_t off = 0;

		if (ext2_map_block(fs, i_block, bi, &phys) != 0 || phys == 0) {
			continue;
		}

		if (ext2_read_block(fs, phys, buf) != BLOCK_OK) {
			continue;
		}

		for (;;) {
			ext2_dirent_hdr_t d;
			int st = ext2_dirent_at(fs, buf, off, &d);

			if (st <= 0) {
				break;
			}

			if (d.inode != 0 &&
				(d.name_len == 0 || d.name_len > d.rec_len - hdr_len)) {
				break;
			}

			if (d.inode != 0 && d.name_len == 2 &&
				buf[off + hdr_len] == '.' && buf[off + hdr_len + 1] == '.') {

				d.inode = parent_inum;
				memcpy(buf + off, &d, hdr_len);

				rc = (ext2_write_block(fs, phys, buf) == BLOCK_OK) ? 0 : -1;
				goto done;
			}

			off += d.rec_len;
		}
	}

done:
	kfree(buf);

	return rc;
}

int ext2_dirent_name_ok(const char* name, size_t name_len) {
	if (!name || name_len == 0 || name_len > EXT2_NAME_LEN) {
		return 0;
	}

	// a name with a slash in it cannot be turned back into a path, and a path is
	// the only way a name is ever looked up again. storing one would leave the
	// directory on disk disagreeing with the namespace above it
	for (size_t i = 0; i < name_len; i++) {
		if (name[i] == '/') {
			return 0;
		}
	}

	return 1;
}

int ext2_dirent_find(ext2_t* fs, const ext2_inode_hdr_t* dir,
	const char* name, size_t name_len, uint32_t* inum) {

	uint32_t hdr_len;
	uint32_t nblocks;
	uint32_t i_block[15];
	uint8_t* buf;
	int rc = -1;

	if (!fs || !dir || !name || !inum) {
		return -1;
	}

	if ((dir->mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
		return -1;
	}

	if (!ext2_dirent_name_ok(name, name_len)) {
		return -1;
	}

	hdr_len = ext2_dirent_hdr_len(fs);

	if ((uint64_t)dir->size > (uint64_t)fs->superblock->blocks_count * fs->block_size) {
		return -1;
	}

	nblocks = (dir->size + fs->block_size - 1) / fs->block_size;
	memcpy(i_block, dir->block, sizeof(i_block));

	buf = kmalloc(fs->block_size, fs->block_size);

	if (!buf) {
		return -1;
	}

	for (uint32_t bi = 0; bi < nblocks && rc != 0; bi++) {
		uint32_t phys = 0;
		uint32_t off = 0;

		if (ext2_map_block(fs, i_block, bi, &phys) != 0 || phys == 0) {
			continue;
		}

		if (ext2_read_block(fs, phys, buf) != BLOCK_OK) {
			continue;
		}

		for (;;) {
			ext2_dirent_hdr_t d;
			int st = ext2_dirent_at(fs, buf, off, &d);

			if (st <= 0) {
				break; // padding or an untrustworthy record, move on
			}

			if (d.inode != 0) {
				if (d.name_len == 0 || d.name_len > d.rec_len - hdr_len) {
					break;
				}

				if (d.name_len == name_len &&
					memcmp(buf + off + hdr_len, name, name_len) == 0) {
					*inum = d.inode;
					rc = 0;
					break;
				}
			}

			off += d.rec_len;
		}
	}

	kfree(buf);

	return rc;
}

int ext2_dirent_is_empty(ext2_t* fs, const ext2_inode_hdr_t* dir, bool* empty) {
	uint32_t hdr_len;
	uint32_t nblocks;
	uint32_t i_block[15];
	uint8_t* buf;
	int rc = -1;

	if (!fs || !dir || !empty) {
		return -1;
	}

	if ((dir->mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
		return -1;
	}

	*empty = true;

	hdr_len = ext2_dirent_hdr_len(fs);

	if ((uint64_t)dir->size > (uint64_t)fs->superblock->blocks_count * fs->block_size) {
		return -1;
	}

	nblocks = (dir->size + fs->block_size - 1) / fs->block_size;
	memcpy(i_block, dir->block, sizeof(i_block));

	buf = kmalloc(fs->block_size, fs->block_size);

	if (!buf) {
		return -1;
	}

	rc = 0;

	for (uint32_t bi = 0; bi < nblocks; bi++) {
		uint32_t phys = 0;
		uint32_t off = 0;

		if (ext2_map_block(fs, i_block, bi, &phys) != 0 || phys == 0) {
			continue;
		}

		if (ext2_read_block(fs, phys, buf) != BLOCK_OK) {
			continue;
		}

		for (;;) {
			ext2_dirent_hdr_t d;
			int st = ext2_dirent_at(fs, buf, off, &d);

			if (st <= 0) {
				break;
			}

			if (d.inode != 0) {
				if (d.name_len == 0 || d.name_len > d.rec_len - hdr_len) {
					// a record we cannot read is a record we cannot clear. saying
					// the directory is empty here would let unlink free an inode
					// that still has a name pointing at it
					*empty = false;
					goto done;
				}

				// "." and ".." are what a directory always has, so they do not
				// count against emptiness
				if (!((d.name_len == 1 && buf[off + hdr_len] == '.') ||
				      (d.name_len == 2 && buf[off + hdr_len] == '.' &&
				       buf[off + hdr_len + 1] == '.'))) {
					*empty = false;
					goto done;
				}
			}

			off += d.rec_len;
		}
	}

done:
	kfree(buf);

	return rc;
}

int ext2_dirs_count_adjust(ext2_t* fs, uint32_t inum, int delta) {
	if (!fs || delta == 0 || inum == 0) {
		return -1;
	}

	uint32_t ipg = fs->superblock->inodes_per_group;
	uint32_t group = (inum - 1) / ipg;

	if (group >= fs->group_count) {
		return -1;
	}

	if (delta > 0) {
		fs->gdt[group].used_dirs_count++;
	} else {
		if (fs->gdt[group].used_dirs_count == 0) {
			return -1;
		}

		fs->gdt[group].used_dirs_count--;
	}

	fs->gdt_dirty = true;

	return 0;
}

int ext2_inode_retire(ext2_t* fs, uint32_t inum) {
	ext2_inode_hdr_t hdr;
	uint32_t nblocks;
	uint32_t last;
	uint32_t freed = 0;
	bool was_dir;

	if (!fs) {
		return -1;
	}

	if (ext2_read_inode(fs, inum, &hdr) != 0) {
		return -1;
	}

	// retiring twice would free the same blocks again, which hands the same
	// physical blocks to two files
	if (hdr.dtime != 0) {
		return -1;
	}

	// read before the header is rewritten, because a retired directory still
	// carries its mode and that is the only hint of what it used to be
	was_dir = (hdr.mode & EXT2_S_IFMT) == EXT2_S_IFDIR;

	nblocks = (hdr.size + fs->block_size - 1) / fs->block_size;
	last = nblocks ? nblocks - 1 : 0;

	// the blocks go first. if this fails the inode is left intact and still
	// linked, which leaks nothing; the other order would leave a name pointing
	// at blocks that no longer belong to it
	if (ext2_free_blocks_from(fs, &hdr, 0, last, &freed) != 0) {
		return -1;
	}

	uint32_t drop = freed * ext2_sectors_per_block(fs);

	hdr.blocks = drop > hdr.blocks ? 0 : hdr.blocks - drop;

	// a deleted inode keeps its mode and size, and is only marked deleted with
	// no links left. zeroing the whole inode instead leaves mode 0 and
	// extra_isize 0, which e2fsck objects to
	hdr.dtime = EXT2_RETIRED_TIME;
	hdr.links_count = 0;

	if (ext2_write_inode(fs, inum, &hdr) != BLOCK_OK) {
		return -1;
	}

	if (was_dir) {
		ext2_dirs_count_adjust(fs, inum, -1);
	}

	return ext2_free_inode(fs, inum);
}

int ext2_sync_metadata(ext2_t* fs) {
	if (!fs || !fs->block_device || !fs->superblock || !fs->gdt) {
		return BLOCK_ERR_INVAL;
	}

	// a read-only mount never dirtied anything and must not write
	if (fs->read_only) {
		return BLOCK_OK;
	}

	int rc = ext2_bitmap_sync(fs, &fs->block_bitmap);

	if (rc == BLOCK_OK) {
		rc = ext2_bitmap_sync(fs, &fs->inode_bitmap);
	}

	if (rc == BLOCK_OK && fs->superblock_dirty) {
		rc = block_write_bytes(fs->block_device, 1024, sizeof(ext2_superblock_t), fs->superblock);

		if (rc == BLOCK_OK) {
			fs->superblock_dirty = false;
		}
	}

	if (rc == BLOCK_OK && fs->gdt_dirty) {
		for (uint32_t i = 0; i < fs->gdt_blocks; i++) {
			rc = ext2_write_block(fs, fs->first_data_block + 1 + i,
								 (uint8_t*)fs->gdt + (uint64_t)i * fs->block_size);

			if (rc != BLOCK_OK) {
				break;
			}
		}

		if (rc == BLOCK_OK) {
			fs->gdt_dirty = false;
		}
	}

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
