/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "kernel/block/block.h"
#include "kernel/block/utils.h"
#include "kernel/handle/inode_type.h"
#include "kernel/handle/registry.h"
#include <kernel/fs/isofs.h>
#include <kernel/handle/fs.h>
#include <panuti/errno.h>
#include <kernel/memman/slab.h>
#include <string.h>

static inline uint16_t read_le16(const uint8_t* p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t read_le32(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool rr_check_sp(const uint8_t* root_dot_record, uint8_t* out_len_skp) {
	uint8_t dr_len = root_dot_record[0];
	uint8_t len_fi = root_dot_record[32];

	uint32_t su_offset = 33 + len_fi;
	if (su_offset & 1) {
		su_offset++;
	}

	if (su_offset + 7 > dr_len) {
		return false; // not enough room for an SP entry at all
	}

	if (root_dot_record[su_offset] == 'S' &&
	    root_dot_record[su_offset + 1] == 'P' &&
	    root_dot_record[su_offset + 2] == 7 &&
	    root_dot_record[su_offset + 4] == 0xBE &&
	    root_dot_record[su_offset + 5] == 0xEF) {
		if (out_len_skp) {
			*out_len_skp = root_dot_record[su_offset + 6];
		}
		
		return true;
	}

	return false;
}

static int rr_apply_name(const uint8_t* dir_record, char* out_name, size_t out_size, const isofs_t* fs) {
	if (!fs->is_rock_ridge) {
		return -1;
	}

	uint8_t dr_len = dir_record[0];
	uint8_t len_fi = dir_record[32];

	uint32_t su_offset = 33 + len_fi;
	if (su_offset & 1) {
		su_offset++; // padding field: present exactly when len_fi is even
	}
	
	su_offset += fs->rr_len_skip;

	size_t name_len = 0;
	bool found = false;

	while (su_offset + 4 <= dr_len) {
		uint8_t sig1 = dir_record[su_offset];
		uint8_t sig2 = dir_record[su_offset + 1];
		uint8_t entry_len = dir_record[su_offset + 2];

		if (entry_len < 4 || su_offset + entry_len > dr_len) {
			break; // malformed SUA
		}

		if (sig1 == 'N' && sig2 == 'M' && entry_len >= 5) {
			uint8_t flags = dir_record[su_offset + 4];
			uint8_t data_len = entry_len - 5; // NM header is SIG+LEN+VER+FLAGS = 5 bytes
			const uint8_t* data = &dir_record[su_offset + 5];

			if (name_len + data_len < out_size) {
				memcpy(out_name + name_len, data, data_len);
				name_len += data_len;
				found = true;
			}

			if (!(flags & 0x01)) {
				break; // bit 0 clear. break up with her.
			}
		}

		su_offset += entry_len;
	}

	if (!found) {
		return -1;
	}

	out_name[name_len] = '\0';
	return 0;
}

static int parse_dirent_basename(
	const uint8_t* dir_record,
	char* out_name,
	size_t out_size,
	const isofs_t* fs
) {
	uint8_t len_fi = dir_record[32];
	const uint8_t* raw_name = &dir_record[33];

	// handle the special children :)
	if (len_fi == 1 && raw_name[0] == 0x00) {
		strcpy(out_name, ".");
		return 0;
	}

	if (len_fi == 1 && raw_name[0] == 0x01) {
		strcpy(out_name, "..");
		return 0;
	}
	
	// prefer the real Rock Ridge filename over ISO9660's 8.3-style name
	if (rr_apply_name(dir_record, out_name, out_size, fs) == 0) {
		return 0;
	}

	if (len_fi >= out_size) {
		return -1;
	}

	memcpy(out_name, raw_name, len_fi);
	out_name[len_fi] = '\0';
	
	// strip the version thing bc who tf uses it
	char* semi = memchr(out_name, ';', len_fi);
	if (semi) {
		*semi = '\0';
	}

	size_t l = strlen(out_name);
	if (l > 0 && out_name[l - 1] == '.') {
		out_name[l - 1] =  '\0';
	}

	return 0;
}

static int parse_dir_record(const uint8_t* dir_record, isofs_dirent_t* out_dirent, const isofs_t* fs) {
	*out_dirent = (isofs_dirent_t){0};

	out_dirent->start_lba = read_le32(&dir_record[2]);
	out_dirent->length = read_le32(&dir_record[10]);
	// this works for bool bc true expands to 1 and false respectively.
	out_dirent->is_dir = (dir_record[25] >> 1) & 1; // 2nd bit flag

	return parse_dirent_basename(dir_record, out_dirent->name, sizeof(out_dirent->name), fs);
}

static inline bool isofs_lba_valid(const isofs_t* fs, uint32_t lba, uint32_t len) {
	// how many ISO blocks does this extent span? (computed in 64 bits so a
	// huge `len` cannot wrap)
	uint64_t blocks = ((uint64_t)len + fs->block_size - 1) / fs->block_size;
	// reject overflow and out-of-range extents
	if (lba >= fs->volume_space_size) return false;
	if (blocks > (uint64_t)fs->volume_space_size - lba) return false; // avoids lba+blocks overflow
	return true;
}

static struct inode* isofs_lookup(void* fs_impl, struct inode* dir, const char* name, size_t len) {
	isofs_t* fs = fs_impl;
	if (dir->type != INODE_DIR) {
		// i hope my code never makes this happen
		return nullptr;
	}

	// what's dir's extent?
	uint32_t sd_lba;
	uint32_t sd_len;
	if (dir->impl) {
		// normal dir
		isofs_dirent_t* di = dir->impl;
		sd_lba = di->start_lba;
		sd_len = di->length;
	} else {
		// fs root
		sd_lba = fs->root.start_lba;
		sd_len = fs->root.length;
	}

	if (!isofs_lba_valid(fs, sd_lba, sd_len)) {
		return nullptr;
	}

	uint8_t* buf = kmalloc(sd_len, 1);
	if (!buf) {
		return nullptr;
	}

	if (block_read_bytes(fs->block_device, (uint64_t)sd_lba * fs->block_size, sd_len, buf) != BLOCK_OK) {
		kfree(buf);
		return nullptr;
	}

	struct inode* result = nullptr;
	uint32_t offset = 0;
	while (offset < sd_len) {
		uint8_t dr_len = buf[offset];

		if (dr_len == 0) {
			break;
		}

		if (dr_len < 34 || (offset + dr_len) > sd_len) {
			offset++;
			continue;
		}

		// the file identifier lives at byte 32 and must fit inside the record
		// itself; without this a hostile record can make us read past `buf`
		if ((uint32_t)buf[offset + 32] > (uint32_t)dr_len - 33) {
			offset++;
			continue;
		}

		isofs_dirent_t dirent;
		if (parse_dir_record(&buf[offset], &dirent, fs) == 0
			&&
			// check if this is what we are looking for
			strlen(dirent.name) == len 
			&&
			strncmp(dirent.name, name, len) == 0
		) {
			inode_t* n = registry_inode_alloc(dirent.is_dir ? INODE_DIR : INODE_FILE);
			if (n) {
				isofs_dirent_t* fs_n = kmalloc(sizeof(isofs_dirent_t), alignof(isofs_dirent_t));
				if (fs_n) {
					fs_n->start_lba = dirent.start_lba;
					fs_n->length = dirent.length;
					fs_n->is_dir = dirent.is_dir;

					n->impl = fs_n;
					n->mnt = dir->mnt;

					if (registry_linkdirent(dir, name, len, n)) {
						result = n;
					} else {
						inode_unref(n);
					}
				} else {
					inode_unref(n);
				}

				break;
			}
		}

		offset += dr_len;
	}

	kfree(buf);
	return result;
}

static void isofs_close(void* file_impl) {
	kfree(file_impl);
}

static void isofs_finish(void* fs_impl) {
	kfree(fs_impl);
}

static void* isofs_open(void* fs_impl, struct inode* node) {
	if (node->type != INODE_FILE) {
		return nullptr;
	}

	isofs_dirent_t* fs_n = node->impl;
	if (!fs_n) {
		return nullptr;
	}

	isofs_file_t* f = kmalloc(sizeof(isofs_file_t), alignof(isofs_file_t));
	if (!f) {
		return nullptr;
	}

	f->fs = fs_impl;
	f->start_lba = fs_n->start_lba;
	f->length = fs_n->length;

	return f;
}

static int isofs_read(void* file_impl, void* buf, size_t len, size_t offset) {
	isofs_file_t* f = file_impl;

	if (offset >= f->length) {
		return 0;
	}
	
	if (len > (f->length - offset)) {
		len = f->length - offset;
	}

	uint64_t b_offset = (uint64_t)f->start_lba * f->fs->block_size + offset;
	if (block_read_bytes(f->fs->block_device, b_offset, len, buf)) {
		return -1;
	}

	return (int)len;
}

// the rest of these ops are absolutely useless just like you, because you're in the wrong place.
// you might be useless as hell in one place, but maybe others will recognize you someplace else.

static int isofs_write(void* file_impl, const void* buf, size_t len, size_t offset) {
	(void)file_impl; (void)buf; (void)len; (void)offset;
	return -1;
}

static int isofs_create(void* fs_impl, struct inode* dir, const char* name, size_t len, inode_type_t type) {
	(void)fs_impl; (void)dir; (void)name; (void)len; (void)type;
	return -1;
}

static int isofs_unlink(void* fs_impl, struct inode* dir, const char* name, size_t len) {
	(void)fs_impl; (void)dir; (void)name; (void)len;
	return -1;
}

static int isofs_readdir(void* fs_impl, struct inode* dir, dirent_entry_t* out, size_t* cursor) {
	isofs_t* fs = fs_impl;

	uint32_t sd_lba;
	uint32_t sd_len;
	if (dir->impl) {
		isofs_dirent_t* di = dir->impl;
		sd_lba = di->start_lba;
		sd_len = di->length;
	} else {
		sd_lba = fs->root.start_lba;
		sd_len = fs->root.length;
	}

	if (!isofs_lba_valid(fs, sd_lba, sd_len)) {
		return -1;
	}

	if (*cursor >= sd_len) {
		return 1; // end of directory
	}

	uint8_t* buf = kmalloc(sd_len, 1);
	if (!buf) {
		return -1;
	}

	if (block_read_bytes(fs->block_device, (uint64_t)sd_lba * fs->block_size, sd_len, buf) != BLOCK_OK) {
		kfree(buf);
		return -1;
	}

	int result = 1; // default: nothing more found
	uint32_t offset = (uint32_t)*cursor;

	while (offset < sd_len) {
		uint8_t dr_len = buf[offset];

		if (dr_len == 0) {
			break; // no more records in this sector
		}

		if (dr_len < 34 || (offset + dr_len) > sd_len) {
			offset++;
			continue;
		}

		if ((uint32_t)buf[offset + 32] > (uint32_t)dr_len - 33) {
			offset++;
			continue;
		}

		isofs_dirent_t dirent;
		if (parse_dir_record(&buf[offset], &dirent, fs) == 0) {
			strncpy(out->name, dirent.name, sizeof(out->name) - 1);
			out->name[sizeof(out->name) - 1] = '\0';
			out->type = dirent.is_dir ? INODE_DIR : INODE_FILE;

			*cursor = offset + dr_len; // resume here next call
			result = 0;
			break;
		}

		offset += dr_len;
	}

	if (result == 1) {
		*cursor = sd_len; // pin the cursor at the end so future calls short-circuit immediately
	}

	kfree(buf);
	return result;
}

static const fs_ops_t isofs_ops = {
	.lookup = isofs_lookup,
	.create = isofs_create,
	.unlink = isofs_unlink,
	.open = isofs_open,
	.read = isofs_read,
	.write = isofs_write,
	.close = isofs_close,
	.finish = isofs_finish,
	.readdir = isofs_readdir,
};

int isofs_mount(const char *mountp, const char *blkdev) {
	block_dev_t* dev = block_find(blkdev);
	if (!dev) {
		return PANUTIERRNO_NOTFOUND;
	}

	uint8_t* desc = kmalloc(ISOFS_BLOCKSIZE, 1);
	if (!desc) {
		return PANUTIERRNO_PLAINERR;
	}
	uint8_t* pvd = kmalloc(ISOFS_BLOCKSIZE, 1);
	if (!pvd) {
		kfree(desc);
		return PANUTIERRNO_PLAINERR;
	}
	bool pvd_found = false;

	for (uint64_t lba = 16; lba < dev->block_count; lba++) {
		int rc = block_read_bytes(dev, lba * ISOFS_BLOCKSIZE, ISOFS_BLOCKSIZE, desc);
		if (rc != BLOCK_OK) {
			kfree(pvd);
			kfree(desc);
			return rc;
		}

		// pack up and get the fuck out of here
		if (desc[0] == 255) {
			break;
		}

		if (desc[0] == 1 && !pvd_found) {
			memcpy(pvd, desc, ISOFS_BLOCKSIZE);
			pvd_found = true;
		}
	}

	if (!pvd_found) {
		kfree(pvd);
		kfree(desc);
		return PANUTIERRNO_PLAINERR;
	}

	// check magic string
	if (memcmp(&pvd[1], "CD001", 5) != 0) {
		kfree(pvd);
		kfree(desc);
		return PANUTIERRNO_PLAINERR;
	}

	// realistically 1 anywhere but just to make sure...
	if (pvd[6] != 1 || pvd[881] != 1) {
		kfree(pvd);
		kfree(desc);
		return PANUTIERRNO_PLAINERR;
	}
	
	isofs_t* fs = kmalloc(sizeof(isofs_t), alignof(isofs_t));
	if (!fs) {
		kfree(pvd);
		kfree(desc);
		return PANUTIERRNO_PLAINERR;
	}
	
	fs->block_device = dev;
	fs->volume_space_size = read_le32(&pvd[80]);
	fs->block_size = read_le16(&pvd[128]);

	// a malformed PVD could claim block_size 0 (division by zero below) or a
	// non-power-of-two size that breaks block arithmetic
	if (fs->block_size == 0 || (fs->block_size & (fs->block_size - 1)) != 0 ||
	    fs->block_size > 65536) {
		kfree(fs);
		kfree(pvd);
		kfree(desc);
		return PANUTIERRNO_PLAINERR;
	}
	
	uint64_t iso_bytes = (uint64_t)fs->volume_space_size * fs->block_size;
	uint64_t dev_bytes = (uint64_t)dev->block_count * dev->block_size;

	if (iso_bytes > dev_bytes) {
		kfree(fs);
		kfree(pvd);
		kfree(desc);
		return PANUTIERRNO_PLAINERR;
	}

	fs->is_rock_ridge = false;
	fs->rr_len_skip = 0;
	
	if (parse_dir_record(&pvd[156], &fs->root, fs) != 0) {
		kfree(fs);
		kfree(pvd);
		kfree(desc);
		return PANUTIERRNO_PLAINERR;
	}
	
	if (!isofs_lba_valid(fs, fs->root.start_lba, fs->root.length)) {
		kfree(fs);
		kfree(pvd);
		kfree(desc);
		return PANUTIERRNO_PLAINERR;
	}

	// the desc buffer is dead after the PVD scan, so reuse it for the root
	// block read and keep the heap footprint to two CD sectors
	if (fs->block_size > ISOFS_BLOCKSIZE) {
		kfree(pvd);
		kfree(desc);
		return PANUTIERRNO_PLAINERR; // shouldn't happen given the block_size cap above, but stay safe
	}

	if (block_read_bytes(
		fs->block_device,
		(uint64_t)fs->root.start_lba * fs->block_size,
		fs->block_size, desc) == BLOCK_OK
	) {
		uint8_t len_skp;
		if (rr_check_sp(desc, &len_skp)) {
			fs->is_rock_ridge = true;
			fs->rr_len_skip = len_skp;
		}
	}
	
	// if this read fails, we just proceed without Rock Ridge rather
	// than failing the whole mount over a cosmetic feature

	kfree(pvd);
	kfree(desc);

	inode_t* mountpoint = registry_resolve(registry_root(), mountp);
	if (!mountpoint || mountpoint->type != INODE_DIR) {
		kfree(fs);
		return PANUTIERRNO_PLAINERR;
	}

	// explicit root inode for the mounted namespace, carrying the root extent
	inode_t* root_node = registry_inode_alloc(INODE_DIR);
	if (!root_node) {
		kfree(fs);
		return PANUTIERRNO_PLAINERR;
	}

	isofs_dirent_t* rd = kmalloc(sizeof(isofs_dirent_t), alignof(isofs_dirent_t));
	if (!rd) {
		inode_unref(root_node);
		kfree(fs);
		return PANUTIERRNO_PLAINERR;
	}
	*rd = fs->root;
	root_node->impl = rd;

	if (mount_attach(mountpoint, &isofs_ops, fs, root_node) != 0) {
		inode_unref(root_node);
		kfree(rd);
		kfree(fs);
		return PANUTIERRNO_PLAINERR;
	}

	return PANUTIERRNO_PLAINSUCCESS;
}