#include <kernel/block/block.h>
#include <kernel/fs/fatfs.h>
#include <kernel/handle/registry.h>
#include <kernel/handle/fs.h>
#include <panuti/errno.h>
#include <kernel/memman/slab.h>
#include <stdalign.h>

static struct inode* fatfs_lookup(void* fs_impl, struct inode* dir, const char* name, size_t len) {
	(void)fs_impl; (void)dir; (void)name; (void)len;
	return NULL;
}

static int fatfs_create(void* fs_impl, struct inode* dir, const char* name, size_t len, inode_type_t type) {
	(void)fs_impl; (void)dir; (void)name; (void)len; (void)type;
	return -1;
}

static int fatfs_unlink(void* fs_impl, struct inode* dir, const char* name, size_t len) {
	(void)fs_impl; (void)dir; (void)name; (void)len;
	return -1;
}

static void* fatfs_open(void* fs_impl, struct inode* node) {
	(void)fs_impl; (void)node;
	return NULL;
}

static int fatfs_read(void* file_impl, void* buf, size_t len, size_t offset) {
	(void)file_impl; (void)buf; (void)len; (void)offset;
	return -1;
}

static int fatfs_write(void* file_impl, const void* buf, size_t len, size_t offset) {
	(void)file_impl; (void)buf; (void)len; (void)offset;
	return -1;
}

static void fatfs_close(void* file_impl) {
	(void)file_impl;
}

static const fs_ops_t fatfs_ops = {
	.lookup = fatfs_lookup,
	.create = fatfs_create,
	.unlink = fatfs_unlink,
	.open = fatfs_open,
	.read = fatfs_read,
	.write = fatfs_write,
	.close = fatfs_close,
};

int fatfs_mount(const char *mountp, const char *blkdev_path) {
	block_dev_t* dev = block_find(blkdev_path);
	if (!dev) {
		return PANUTIERRNO_NOTFOUND;
	}

	uint8_t* bpb = kmalloc(dev->block_size, 1);
	if (!bpb) {
		return PANUTIERRNO_PLAINERR;
	}

	if (dev->ops->read(dev->impl, 0, bpb, 1) != BLOCK_OK) {
		kfree(bpb);
		return PANUTIERRNO_PLAINERR;
	}

	uint16_t bps = bpb[11] | ((uint16_t)bpb[12] << 8);
	uint8_t spc = bpb[13];
	uint16_t rsvd = bpb[14] | ((uint16_t)bpb[15] << 8);
	uint8_t nfats = bpb[16];
	uint16_t root_ent = bpb[17] | ((uint16_t)bpb[18] << 8);
	uint32_t tot_sec16 = bpb[19] | ((uint32_t)bpb[20] << 8);
	uint16_t fat_sz16 = bpb[22] | ((uint16_t)bpb[23] << 8);
	uint32_t tot_sec32 = (uint32_t)bpb[32] | ((uint32_t)bpb[33] << 8) | ((uint32_t)bpb[34] << 16) | ((uint32_t)bpb[35] << 24);
	uint32_t fat_sz32 = (uint32_t)bpb[36] | ((uint32_t)bpb[37] << 8) | ((uint32_t)bpb[38] << 16) | ((uint32_t)bpb[39] << 24);
	uint32_t root_clus = (uint32_t)bpb[44] | ((uint32_t)bpb[45] << 8) | ((uint32_t)bpb[46] << 16) | ((uint32_t)bpb[47] << 24);

	if (bps != dev->block_size || spc == 0 || nfats == 0 || (spc & (spc - 1)) != 0) {
		kfree(bpb);
		return PANUTIERRNO_PLAINERR;
	}

	uint32_t tot_sec = tot_sec16 ? tot_sec16 : tot_sec32;
	uint32_t fat_sz = fat_sz16;
	uint32_t root_dir_sectors = ((uint32_t)root_ent * 32 + bps - 1) / bps;
	uint32_t root_cluster = 0;

	uint32_t meta = rsvd + (uint32_t)nfats * fat_sz + root_dir_sectors;
	if (tot_sec == 0 || meta >= tot_sec) {
		kfree(bpb);
		return PANUTIERRNO_PLAINERR;
	}

	uint32_t data_sec = tot_sec - meta;
	uint32_t clusters = data_sec / spc;

	if (clusters >= 65525) {
		fat_sz = fat_sz32;
		root_cluster = root_clus;
		root_dir_sectors = 0;

		meta = rsvd + (uint32_t)nfats * fat_sz;
		if (fat_sz == 0 || meta >= tot_sec) {
			kfree(bpb);
			return PANUTIERRNO_PLAINERR;
		}

		data_sec = tot_sec - meta;
		clusters = data_sec / spc;
	}

	if (clusters == 0) {
		kfree(bpb);
		return PANUTIERRNO_PLAINERR;
	}

	fatfs_t* fs = kmalloc(sizeof(fatfs_t), alignof(fatfs_t));
	if (!fs) {
		kfree(bpb);
		return PANUTIERRNO_PLAINERR;
	}

	if (clusters < 4085) {
		fs->type = FATFSTY_FAT12;
	} else if (clusters < 65525) {
		fs->type = FATFSTY_FAT16;
	} else {
		fs->type = FATFSTY_FAT32;
	}

	fs->blk_dev = dev;
	fs->bp_sector = bps;
	fs->bp_cluster = (uint32_t)spc * bps;
	fs->reserved_sectors = rsvd;
	fs->fat_count = nfats;
	fs->fat_start_sec = rsvd;
	fs->fat_size = fat_sz;
	fs->data_start_sec = rsvd + (uint32_t)nfats * fat_sz + root_dir_sectors;
	fs->total_clusters = clusters;
	fs->root_cluster = root_cluster;
	fs->root_dir_start_sec = rsvd + (uint32_t)nfats * fat_sz;
	fs->root_dir_size = root_dir_sectors;

	fs->fat_cache = kmalloc((size_t)fat_sz * bps, 1);
	if (!fs->fat_cache) {
		kfree(fs);
		kfree(bpb);
		return PANUTIERRNO_PLAINERR;
	}

	if (dev->ops->read(dev->impl, fs->fat_start_sec, fs->fat_cache, fat_sz) != BLOCK_OK) {
		kfree(fs->fat_cache);
		kfree(fs);
		kfree(bpb);
		return PANUTIERRNO_PLAINERR;
	}

	kfree(bpb);

	if (registry_mount(mountp, &fatfs_ops, fs) != 0) {
		kfree(fs->fat_cache);
		kfree(fs);
		return PANUTIERRNO_PLAINERR;
	}

	return 0;
}
