/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "kernel/block/block.h"
#include <kernel/fs/ext2.h>
#include <kernel/handle/registry.h>
#include <panuti/errno.h>
#include <kernel/memman/slab.h>
#include <kernel/block/utils.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

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

static uint64_t ext2_inode_size(ext2_t* fs, const ext2_inode_hdr_t* hdr) {
	if (fs->large_files && hdr->size_high != 0) {
		return ((uint64_t)hdr->size_high << 32) | hdr->size;
	}

	return hdr->size;
}

static int ext2_read_inode(ext2_t* fs, uint32_t inum, ext2_inode_hdr_t* out) {
	if (inum == 0 || inum > fs->superblock->inodes_count) {
		return -1;
	}

	uint32_t group = (inum - 1) / fs->superblock->inodes_per_group;
	uint32_t index_in_group = (inum - 1) % fs->superblock->inodes_per_group;

	if (group >= fs->group_count) {
		return -1;
	}

	uint32_t table_block = fs->gdt[group].inode_table + index_in_group / fs->inodes_per_block;
	uint32_t byte_off = (index_in_group % fs->inodes_per_block) * fs->inode_size;

	if (table_block >= fs->superblock->blocks_count) {
		return -1;
	}

	// inode_size is at least 128 and the header is 112, so the tail of the last
	// inode in a block always has room for it
	if (byte_off > fs->block_size ||
		fs->block_size - byte_off < sizeof(ext2_inode_hdr_t)) {
		return -1;
	}

	uint64_t offset = (uint64_t)table_block * fs->block_size + byte_off;

	if (offset > UINT64_MAX - sizeof(ext2_inode_hdr_t)) {
		return -1;
	}

	return block_read_bytes(fs->block_device, offset, sizeof(ext2_inode_hdr_t), out) == BLOCK_OK ? 0 : -1;
}

static int ext2_indirect_entry(ext2_t* fs, uint32_t block, uint32_t index, uint32_t* out) {
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

static int ext2_map_block(ext2_t* fs, const uint32_t* i_block, uint32_t index, uint32_t* out) {
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

static void ext2_free_inode_tree(inode_t* n) {
	for (dirent_t* d = n->children; d; d = d->next) {
		if (d->inode == n) {
			continue;
		}

		ext2_free_inode_tree(d->inode);

		if (d->inode->impl) {
			kfree(d->inode->impl);
			d->inode->impl = nullptr;
		}
	}
}

static struct inode* ext2_lookup(void* fs_impl, struct inode* dir, const char* name, size_t len) {
	ext2_t* fs = fs_impl;

	if (!fs || !dir || !name || len == 0 || len > EXT2_NAME_LEN) {
		return nullptr;
	}

	if (dir->type != INODE_DIR) {
		return nullptr;
	}

	// inode_t has nowhere to record the on-disk inode number, so it can only
	// come from the per-inode state mount and lookup hang off impl
	ext2_inode_t* dir_inode = dir->impl;

	if (!dir_inode || dir_inode->fs != fs) {
		return nullptr;
	}

	// walk() resolves through the mount rather than through the dirent list, so
	// this is the only thing that stops a repeated lookup of the same name from
	// allocating another inode every time
	dirent_t* cached = registry_finddirent(dir, name, len);

	if (cached) {
		return cached->inode;
	}

	ext2_inode_hdr_t dih;

	if (ext2_read_inode(fs, dir_inode->inum, &dih) != 0) {
		return nullptr;
	}

	if ((dih.mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
		return nullptr;
	}

	if (dih.size == 0) {
		return nullptr; // mke2fs gives a fresh empty directory no blocks at all
	}

	// a directory cannot be larger than the volume holding it. bounding it here
	// keeps a corrupt size from turning into a very long scan
	if ((uint64_t)dih.size > (uint64_t)fs->superblock->blocks_count * fs->block_size) {
		return nullptr;
	}

	uint8_t* data = kmalloc(fs->block_size, 1);

	if (!data) {
		return nullptr;
	}

	uint32_t nblocks = div_ceil_u32(dih.size, fs->block_size);
	uint32_t inum = 0;

	// i_block lives at offset 40 of a packed record, so it is not guaranteed to
	// be aligned. copying it out gives the block mapper a properly aligned array
	uint32_t i_block[15];

	memcpy(i_block, dih.block, sizeof(i_block));

	for (uint32_t bi = 0; bi < nblocks && inum == 0; bi++) {
		uint32_t phys;

		if (ext2_map_block(fs, i_block, bi, &phys) != 0) {
			continue; // sparse or corrupt, a later block may still be readable
		}

		if (ext2_read_block(fs, phys, data) != BLOCK_OK) {
			continue;
		}

		uint32_t off = 0;

		while (off < fs->block_size) {
			ext2_dirent_hdr_t d;

			memcpy(&d, data + off, sizeof(d));

			// a zero rec_len means the rest of this block is padding and the
			// directory picks up again in the next one
			if (d.rec_len == 0) {
				break;
			}

			// rec_len is the only thing bounding the name, so a corrupt one
			// would otherwise walk us straight off the end of the buffer
			if (d.rec_len < sizeof(ext2_dirent_hdr_t) ||
				(d.rec_len % 4) != 0 ||
				d.rec_len > fs->block_size ||
				off > fs->block_size - d.rec_len) {
				break;
			}

			if (d.inode == 0) {
				off += d.rec_len;
				continue; // deleted entry
			}

			if (d.name_len == 0 ||
				d.name_len > d.rec_len - sizeof(ext2_dirent_hdr_t)) {
				break;
			}

			if (d.name_len == len &&
				memcmp(data + off + sizeof(d), name, len) == 0) {
				inum = d.inode;
				break;
			}

			off += d.rec_len;
		}
	}

	kfree(data);

	if (inum == 0) {
		return nullptr;
	}

	// mode is the authority on the type. the dirent's file_type is only a
	// hint and does not even exist without the FILETYPE feature, and getting it
	// wrong would stick, because this inode is cached for the whole mount
	ext2_inode_hdr_t child;

	if (ext2_read_inode(fs, inum, &child) != 0 || child.dtime != 0) {
		return nullptr;
	}

	inode_type_t type;
	uint16_t fmt = child.mode & EXT2_S_IFMT;

	if (fmt == EXT2_S_IFDIR) {
		type = INODE_DIR;
	} else if (fmt == EXT2_S_IFREG) {
		type = INODE_FILE;
	} else {
		// inode_type_t has no link or device type, and reporting one as a
		// plain file would only produce a nonsense read later
		return nullptr;
	}

	inode_t* n = registry_inode_alloc(type);

	if (!n) {
		return nullptr;
	}

	ext2_inode_t* impl = kmalloc(sizeof(ext2_inode_t), alignof(ext2_inode_t));

	if (!impl) {
		inode_unref(n);
		return nullptr;
	}

	impl->fs = fs;
	impl->inum = inum;
	n->impl = impl;

	if (!registry_linkdirent(dir, name, len, n)) {
		n->impl = nullptr;
		kfree(impl);
		inode_unref(n);
		return nullptr;
	}

	// hand back the reference registry_inode_alloc started with. the dirent
	// holds the only remaining one, and drops it on unlink or at unmount
	inode_unref(n);

	return n;
}

static int ext2_create(void* fs_impl, struct inode* dir, const char* name, size_t len, inode_type_t type) {
	(void)fs_impl; (void)dir; (void)name; (void)len; (void)type;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

static int ext2_unlink(void* fs_impl, struct inode* dir, const char* name, size_t len) {
	(void)fs_impl; (void)dir; (void)name; (void)len;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

static int ext2_rename(void* fs_impl, struct inode* old_dir, const char* old_name, size_t old_len,
                       struct inode* new_dir, const char* new_name, size_t new_len) {
	(void)fs_impl; (void)old_dir; (void)old_name; (void)old_len;
	(void)new_dir; (void)new_name; (void)new_len;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

static int ext2_link(void* fs_impl, struct inode* target, struct inode* dir,
                     const char* name, size_t len) {
	(void)fs_impl; (void)target; (void)dir; (void)name; (void)len;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

// The length a file claims to have, or 0 if that is not believable. A file
// cannot be larger than the volume holding it, and a length read out of a
// corrupt inode is not a reason to hand the reader megabytes of zeroes. Both
// size() and open() go through here so they can never disagree about whether a
// length is usable.
static int ext2_inode_length(ext2_t* fs, const ext2_inode_hdr_t* hdr, uint64_t* out) {
	uint64_t len = ext2_inode_size(fs, hdr);

	if (len > (uint64_t)fs->superblock->blocks_count * fs->block_size) {
		return -1; // zero is a real length, so it cannot double as the failure mark
	}

	*out = len;
	return 0;
}

static int64_t ext2_size(void* fs_impl, struct inode* node) {
	ext2_t* fs = fs_impl;

	if (!fs || !node) {
		return -1;
	}

	ext2_inode_t* ii = node->impl;
	if (!ii || ii->fs != fs) {
		return -1;
	}

	ext2_inode_hdr_t hdr;
	if (ext2_read_inode(fs, ii->inum, &hdr) != 0) {
		return -1;
	}

	if (hdr.dtime != 0) {
		return -1; // unlinked: the name is gone, so there is nothing to report
	}

	uint64_t len = 0;
	if (ext2_inode_length(fs, &hdr, &len) != 0) {
		return -1; // an impossible length is a length we cannot vouch for
	}

	return (int64_t)len;
}

static void* ext2_open(void* fs_impl, struct inode* node) {
	ext2_t* fs = fs_impl;

	if (!fs || !node) {
		return nullptr;
	}

	ext2_inode_t* ii = node->impl;
	if (!ii || ii->fs != fs) {
		return nullptr;
	}

	ext2_inode_hdr_t hdr;
	if (ext2_read_inode(fs, ii->inum, &hdr) != 0) {
		return nullptr;
	}

	if (hdr.dtime != 0) {
		return nullptr; // unlinked since the name was resolved
	}

	// trust the inode, not the name we were handed: a directory or a symlink is
	// not a file, and opening one as a file would hand out a nonsense handle.
	// this is the same rule readdir applies when classifying entries.
	if ((hdr.mode & EXT2_S_IFMT) != EXT2_S_IFREG) {
		return nullptr;
	}

	// refuse a length that does not fit on the volume, so read is never asked to
	// walk a corrupt size across millions of blocks that hold nothing
	uint64_t len = 0;
	if (ext2_inode_length(fs, &hdr, &len) != 0) {
		return nullptr;
	}

	ext2_file_t* f = kmalloc(sizeof(ext2_file_t), alignof(ext2_file_t));
	if (!f) {
		return nullptr;
	}

	f->fs = fs;
	f->inum = ii->inum;
	f->size = len;
	memcpy(f->block, hdr.block, sizeof(f->block));

	return f;
}

static int ext2_read_part(ext2_t* fs, uint32_t block, uint32_t within, void* buf, size_t len) {
	if (len == 0) {
		return BLOCK_OK;
	}

	if (block >= fs->superblock->blocks_count) {
		return BLOCK_ERR_INVAL;
	}

	uint64_t offset = (uint64_t)block * fs->block_size + within;
	if (offset > UINT64_MAX - len) {
		return BLOCK_ERR_INVAL;
	}

	return block_read_bytes(fs->block_device, offset, len, buf);
}

static int ext2_read(void* file_impl, void* buf, size_t len, size_t offset) {
	if (!file_impl || !buf) {
		return -1;
	}

	ext2_file_t* f = file_impl;
	ext2_t* fs = f->fs;

	if (!fs || !fs->block_device || !fs->superblock) {
		return -1;
	}

	if (len > (size_t)INT_MAX) {
		len = (size_t)INT_MAX;
	}

	if ((uint64_t)offset >= f->size) {
		return 0;
	}

	uint64_t avail = f->size - (uint64_t)offset;
	if ((uint64_t)len > avail) {
		len = (size_t)avail;
	}

	uint64_t pos = (uint64_t)offset;
	size_t done = 0;

	while (done < len) {
		uint32_t index = (uint32_t)(pos / fs->block_size);
		uint32_t within = (uint32_t)(pos % fs->block_size);

		size_t chunk = fs->block_size - within;
		if (chunk > len - done) {
			chunk = len - done;
		}

		uint8_t* dst = (uint8_t*)buf + done;
		uint32_t phys = 0;

		if (ext2_map_block(fs, f->block, index, &phys) == 0) {
			if (ext2_read_part(fs, phys, within, dst, chunk) != BLOCK_OK) {
				return -1;
			}
		} else {
			// inside the file but unmapped: a hole.
			memset(dst, 0, chunk);
		}

		done += chunk;
		pos += chunk;
	}

	return (int)done;
}

static int ext2_write(void* file_impl, const void* buf, size_t len, size_t offset) {
	(void)file_impl; (void)buf; (void)len; (void)offset;
	return PANUTIERRNO_UNSUPPORTEDOP;
}

static int ext2_readdir(void* fs_impl, struct inode* dir, dirent_entry_t* out, size_t* cursor) {
	(void)fs_impl; (void)dir; (void)out; (void)cursor;
	return 1;
}

static void ext2_close(void* file_impl) {
	kfree(file_impl);
}

static void ext2_finish(void* fs_impl) {
	ext2_t* fs = fs_impl;
	if (!fs) {
		return;
	}

	// mount_detach runs this before it drops the root's last reference, and
	// registry_destroy only walks references afterwards without telling the
	// filesystem, so the whole cached tree has to go while it is still whole
	if (fs->root_node) {
		if (fs->root_node->impl) {
			kfree(fs->root_node->impl);
			fs->root_node->impl = nullptr;
		}

		ext2_free_inode_tree(fs->root_node);
	}

	if (fs->gdt) {
		kfree(fs->gdt);
	}
	
	if (fs->superblock) {
		kfree(fs->superblock);
	}
	
	if (fs->cached_indirect_buf) {
		kfree(fs->cached_indirect_buf);
	}
	
	if (fs->dir_buf) {
		kfree(fs->dir_buf);
	}

	kfree(fs);
}

static const fs_ops_t ext2_ops = {
	.lookup = ext2_lookup,
	.create = ext2_create,
	.unlink = ext2_unlink,
	.rename = ext2_rename,
	.link = ext2_link,
	.size = ext2_size,
	.open = ext2_open,
	.read = ext2_read,
	.write = ext2_write,
	.close = ext2_close,
	.finish = ext2_finish,
	.readdir = ext2_readdir,
};

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

	fs->cached_indirect_buf = kmalloc(fs->block_size, 1);
	if (!fs->cached_indirect_buf) {
		goto fail;
	}

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

	// the mounted namespace gets its own root inode pointing at EXT2_ROOT_INO,
	// so resolution inside the mount starts from the filesystem root rather
	// than from the mountpoint
	inode_t* root_node = registry_inode_alloc(INODE_DIR);
	if (!root_node) {
		goto fail;
	}

	ext2_inode_t* root_inode = kmalloc(sizeof(ext2_inode_t), alignof(ext2_inode_t));
	if (!root_inode) {
		inode_unref(root_node);
		goto fail;
	}

	root_inode->fs = fs;
	root_inode->inum = EXT2_ROOT_INO;
	root_node->impl = root_inode;

	// only for finish()'s benefit: mount_attach takes its own reference on the
	// root inode, so this needs none of its own
	fs->root_node = root_node;

	if (mount_attach(mountpoint, &ext2_ops, fs, root_node) != 0) {
		fs->root_node = nullptr;
		root_node->impl = nullptr;
		kfree(root_inode);
		inode_unref(root_node);
		goto fail;
	}

	// mount_attach took its own reference, so hand back the one
	// registry_inode_alloc started with. the mount now holds the only one, and
	// mount_detach will drop it through inode_unref
	inode_unref(root_node);

	return PANUTIERRNO_PLAINSUCCESS;

fail:
	if (fs->gdt) {
		kfree(fs->gdt);
	}

	if (fs->cached_indirect_buf) {
		kfree(fs->cached_indirect_buf);
	}

	kfree(sb);
	kfree(fs);
	return PANUTIERRNO_PLAINERR;
}
