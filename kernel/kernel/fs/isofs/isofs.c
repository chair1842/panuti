#include "kernel/block/block.h"
#include "kernel/block/utils.h"
#include "kernel/handle/inode_type.h"
#include "kernel/handle/registry.h"
#include <kernel/fs/isofs.h>
#include <kernel/handle/fs.h>
#include <panuti/errno.h>
#include <stdalign.h>
#include <kernel/memman/slab.h>
#include <string.h>

static inline uint16_t read_le16(const uint8_t* p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t read_le32(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int parse_dirent_basename(const uint8_t* dir_record, char* out_name, size_t out_size) {
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

static int parse_dir_record(const uint8_t* dir_record, isofs_dirent_t* out_dirent) {
	*out_dirent = (isofs_dirent_t){0};

	out_dirent->start_lba = read_le32(&dir_record[2]);
	out_dirent->length = read_le32(&dir_record[10]);
	// this works for bool bc true expands to 1 and false respectively.
	out_dirent->is_dir = (dir_record[25] >> 1) & 1; // 2nd bit flag

	return parse_dirent_basename(dir_record, out_dirent->name, sizeof(out_dirent->name));
}

static inline bool isofs_lba_valid(const isofs_t* fs, uint32_t lba, uint32_t len) {
	// how many ISO blocks does this extent span?
	uint32_t blocks = (len + fs->block_size - 1) / fs->block_size;
	// reject overflow and out-of-range extents
	if (lba >= fs->volume_space_size) return false;
	if (blocks > fs->volume_space_size - lba) return false; // avoids lba+blocks overflow
	return true;
}

static struct inode* isofs_lookup(void* fs_impl, struct inode* dir, const char* name, size_t len) {
	isofs_t* fs = fs_impl;
	if (dir->type != INODE_DIR) {
		// i hope my code never makes this happen
		return NULL;
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
		return NULL;
	}

	uint8_t* buf = kmalloc(sd_len, 1);
	if (!buf) {
		return NULL;
	}

	if (block_read_bytes(fs->block_device, (uint64_t)sd_lba * fs->block_size, sd_len, buf) != BLOCK_OK) {
		kfree(buf);
		return NULL;
	}

	struct inode* result = NULL;
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

		isofs_dirent_t dirent;
		if (
			parse_dir_record(&buf[offset], &dirent) == 0
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
					n->fs_ops = dir->fs_ops;
					n->fs_impl = dir->fs_impl;

					if (registry_linkdirent(dir, name, len, n)) {
						result = n;
					} else {
						inode_unref(n);
					}
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

static void* isofs_open(void* fs_impl, struct inode* node) {
	if (node->type != INODE_FILE) {
		return NULL;
	}

	isofs_dirent_t* fs_n = node->impl;
	if (!fs_n) {
		return NULL;
	}

	isofs_file_t* f = kmalloc(sizeof(isofs_file_t), alignof(isofs_file_t));
	if (!f) {
		return NULL;
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

	uint64_t b_offset = (uint64_t)((f->start_lba * f->fs->block_size) + offset);
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

static const fs_ops_t isofs_ops = {
	.lookup = isofs_lookup,
	.create = isofs_create,
	.unlink = isofs_unlink,
	.open = isofs_open,
	.read = isofs_read,
	.write = isofs_write,
	.close = isofs_close,
};

int isofs_mount(const char *mountp, const char *blkdev) {
	block_dev_t* dev = block_find(blkdev);
	if (!dev) {
		return PANUTIERRNO_NOTFOUND;
	}

	uint8_t pvd[ISOFS_BLOCKSIZE];
	bool pvd_found = false;

	for (uint64_t lba = 16; lba < dev->block_count; lba++) {
		uint8_t desc[ISOFS_BLOCKSIZE];
		int rc = block_read_bytes(dev, lba * ISOFS_BLOCKSIZE, ISOFS_BLOCKSIZE, desc);
		if (rc != BLOCK_OK) {
			return rc;
		}

		// pack up and get the fuck out of here
		if (desc[0] == 255) {
			break;
		}

		if (desc[0] == 1 && !pvd_found) {
			memcpy(pvd, desc, sizeof(desc));
			pvd_found = true;
		}
	}

	if (!pvd_found) {
		return PANUTIERRNO_PLAINERR;
	}

	// check magic string
	if (memcmp(&pvd[1], "CD001", 5) != 0) {
		return PANUTIERRNO_PLAINERR;
	}

	// realistically 1 anywhere but just to make sure...
	if (pvd[6] != 1 || pvd[881] != 1) {
		return PANUTIERRNO_PLAINERR;
	}
	
	isofs_t* fs = kmalloc(sizeof(isofs_t), alignof(isofs_t));
	if (!fs) {
		return PANUTIERRNO_PLAINERR;
	}
	
	fs->block_device = dev;
	fs->volume_space_size = read_le32(&pvd[80]);
	fs->block_size = read_le16(&pvd[128]);
	
	uint64_t iso_bytes = (uint64_t)fs->volume_space_size * fs->block_size;
	uint64_t dev_bytes = (uint64_t)dev->block_count * dev->block_size;

	if (iso_bytes > dev_bytes) {
		kfree(fs);
		return PANUTIERRNO_PLAINERR;
	}
	
	if (parse_dir_record(&pvd[156], &fs->root) != 0) {
		kfree(fs);
		return PANUTIERRNO_PLAINERR;
	}
	
	if (!isofs_lba_valid(fs, fs->root.start_lba, fs->root.length)) {
		kfree(fs);
		return PANUTIERRNO_PLAINERR;
	}
		
	if (registry_mount(mountp, &isofs_ops, fs) != 0) {
		kfree(fs);
		return PANUTIERRNO_PLAINERR;
	}

	return PANUTIERRNO_PLAINSUCCESS;
}