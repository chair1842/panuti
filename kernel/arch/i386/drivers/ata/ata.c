/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ide.h"
#include <stdint.h>
#include <stddef.h>
#include "../../io.h"
#include <kernel/ata/ata.h>
#include <kernel/block/block.h>
#include <kernel/memman/slab.h>
#include <kernel/klog.h>
#include <string.h>

// lba28 tops out here: 28 address bits, 512 byte sectors
#define ATA_MAX_LBA28 0x0FFFFFFFULL

// sectors per command. the taskfile holds a count in 8 bits (0 meaning 256),
// but 128 keeps the chunks a power of two and comfortably under the limit
#define ATA_CHUNK_SECTORS 128

// rep insw/outsw take their counter in cx, so a single rep is capped at 65535
// words. 32768 leaves headroom and lands on a tidy 64 KiB
#define ATA_MAX_TRANSFER_WORDS 32768

// MBR: a 512 byte sector whose last two bytes are 0x55 0xAA, with four
// 16 byte partition entries starting at offset 0x1BE
#define MBR_SIZE 512
#define MBR_SIGNATURE 0xAA55
#define MBR_TABLE_OFFSET 0x1BE
#define MBR_ENTRY_SIZE 16
#define MBR_MAX_PARTITIONS 4

#define MBR_TYPE_EMPTY 0x00
#define MBR_TYPE_GPT_PROTECTIVE 0xEE

// ATA strings in the identify data are byte swapped pairs with the trailing
// space and nul already stripped. the first character of the string sits in
// the *high* byte of each word, so read it back out in that order
static void ata_string(const uint16_t* words, size_t first_word, size_t word_count, char* out, size_t out_size) {
	size_t o = 0;

	for (size_t i = 0; i < word_count && o + 1 < out_size; i++) {
		uint16_t w = words[first_word + i];
		char bytes[3] = { (char)(w >> 8), (char)(w & 0xFF), '\0' };

		for (size_t b = 0; b < 2 && o + 1 < out_size; b++) {
			if (bytes[b] == '\0') {
				bytes[b] = ' ';
			}
			out[o++] = bytes[b];
		}
	}

	out[o] = '\0';

	// trim the trailing padding the spec leaves behind
	while (o > 0 && out[o - 1] == ' ') {
		out[--o] = '\0';
	}
}

// pull the geometry out of an identify payload and stash it on the drive
static void ata_apply_identify(ide_drive_t* drv, const uint16_t* words) {
	// word 49 bit 9: does this drive do 48 bit addressing?
	drv->lba48 = (words[49] & 0x0200) != 0;

	// word 83 bit 10: writes land in a cache the drive may not have flushed
	drv->volatile_write_cache = (words[83] & 0x0400) != 0;
	drv->has_flush = (words[83] & 0x1000) != 0;

	// words 60-61: addressable sectors as the drive sees it under lba28
	uint32_t lo28 = (uint32_t)words[60] | ((uint32_t)words[61] << 16);
	if (lo28 > ATA_MAX_LBA28) {
		lo28 = ATA_MAX_LBA28;
	}

	uint64_t sectors = lo28;

	if (drv->lba48) {
		// words 100-103: the total the drive counts in 48 bit space. prefer it
		// whenever it is believable, *not* only when its top half is nonzero:
		// the upper 16 bits are zero for every disk under 128 PiB, so a naive
		// "is the high word set" test silently caps a 130 GiB drive at 128 GiB.
		//
		// drives that advertise lba48 without implementing the pair leave these
		// words at zero or all ones, and a 48 bit total below the 28 bit one is
		// nonsense, so treat those as unimplemented
		uint64_t total = (uint64_t)words[100] | ((uint64_t)words[101] << 16) |
			((uint64_t)words[102] << 32) | ((uint64_t)words[103] << 48);

		if (total != 0 && total != 0x0000FFFFFFFFFFFFULL && total >= sectors) {
			sectors = total;
		}
	}

	// words 106-107: logical sector size. 4096 byte advanced format drives lie
	// to anyone who assumes 512, so trust the drive
	drv->sector_size = (uint32_t)words[106] | ((uint32_t)words[107] << 16);
	if (drv->sector_size != 512 && drv->sector_size != 1024 &&
	    drv->sector_size != 2048 && drv->sector_size != 4096) {
		drv->sector_size = 512;
	}

	drv->block_count = sectors;
}

static inline uint32_t read_le32(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3]);
}

static uint8_t ata_head(ide_drive_t* drv) {
	return drv->index ? (ATA_HEAD_SLAVE | ATA_HEAD_LBA) : (ATA_HEAD_MASTER | ATA_HEAD_LBA);
}

static void ata_hob(ide_channel_t* ch, bool enable) {
	uint8_t v = ATA_DCR_NIEN;
	if (enable) {
		v |= ATA_DCR_HOB;
	}
	outb(ch->ctrl_base + ATA_REG_DEV_CTRL, v);
}

// report whatever the drive put in the error register, then swallow it
static void ata_report(ide_drive_t* drv, const char* what) {
	ide_channel_t* ch = drv->channel;
	klog(KLOG_WARN, "ata: %s failed on 0x%x %s: %s\n", what, ch->io_base,
		 drv->index ? "slave" : "master",
		 ide_error_string(ide_read_reg(ch, ATA_REG_ERROR)));
}

static void ata_setup_taskfile(ide_drive_t* drv, uint64_t lba, size_t sectors, bool use_lba48) {
	ide_channel_t* ch = drv->channel;

	if (use_lba48) {
		ata_hob(ch, true);

		ide_write_reg(ch, ATA_REG_FEATURES, 0x00);
		ide_write_reg(ch, ATA_REG_SECCOUNT, (uint8_t)(sectors >> 8));
		ide_write_reg(ch, ATA_REG_LBA_LO, (uint8_t)(lba >> 24));
		ide_write_reg(ch, ATA_REG_LBA_MID, (uint8_t)(lba >> 32));
		ide_write_reg(ch, ATA_REG_LBA_HI, (uint8_t)(lba >> 40));
		ide_write_reg(ch, ATA_REG_DRIVE_HEAD, ata_head(drv) | (uint8_t)((lba >> 48) & 0x0F));

		ata_hob(ch, false);
	}

	ide_write_reg(ch, ATA_REG_FEATURES, 0x00);
	ide_write_reg(ch, ATA_REG_SECCOUNT, (uint8_t)sectors);
	ide_write_reg(ch, ATA_REG_LBA_LO, (uint8_t)lba);
	ide_write_reg(ch, ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
	ide_write_reg(ch, ATA_REG_LBA_HI, (uint8_t)(lba >> 16));
	ide_write_reg(ch, ATA_REG_DRIVE_HEAD, ata_head(drv) | (uint8_t)((lba >> 24) & 0x0F));
}

static void ata_issue(ide_drive_t* drv, uint8_t command) {
	ide_write_reg(drv->channel, ATA_REG_COMMAND, command);
}

// one chunk, one command. read pulls sectors in, write pushes them out
static int ata_xfer_chunk(ide_drive_t* drv, uint64_t lba, void* buf, size_t sectors, bool write) {
	ide_channel_t* ch = drv->channel;
	bool use_lba48 = drv->lba48 && lba > ATA_MAX_LBA28;
	size_t words = drv->sector_size / 2;

	if (ide_wait_ready(ch, ATA_TIMEOUT_TICKS) < 0) {
		klog(KLOG_WARN, "ata: drive stuck busy on 0x%x %s\n", ch->io_base,
			 drv->index ? "slave" : "master");
		return BLOCK_ERR_IO;
	}

	ata_setup_taskfile(drv, lba, sectors, use_lba48);

	uint8_t command;
	if (write) {
		command = use_lba48 ? ATA_CMD_WRITE_SECTORS_EXT : ATA_CMD_WRITE_SECTORS;
	} else {
		command = use_lba48 ? ATA_CMD_READ_SECTORS_EXT : ATA_CMD_READ_SECTORS;
	}

	ata_issue(drv, command);

	int status = ide_wait_drq(ch, ATA_TIMEOUT_TICKS);
	if (status < 0) {
		klog(KLOG_WARN, "ata: no data phase on 0x%x %s, lba %llu\n", ch->io_base,
			 drv->index ? "slave" : "master", (unsigned long long)lba);
		return BLOCK_ERR_IO;
	}

	if (status & ATA_SR_ERR) {
		ata_report(drv, write ? "WRITE SECTORS" : "READ SECTORS");
		return BLOCK_ERR_IO;
	}

	// the drive streams one sector per data phase and only re-asserts DRQ once
	// the sector we just took is fully consumed, so a multi sector command has
	// to be drained a sector at a time. doing it in one rep string would read
	// past the buffer the drive has queued up for us
	for (size_t i = 0; i < sectors; i++) {
		uint8_t* p = (uint8_t*)buf + (i * drv->sector_size);

		if (i > 0) {
			status = ide_wait_drq(ch, ATA_TIMEOUT_TICKS);
			if (status < 0) {
				klog(KLOG_WARN, "ata: transfer stalled after %zu of %zu sectors on 0x%x %s\n",
						i, sectors, ch->io_base, drv->index ? "slave" : "master");
				return BLOCK_ERR_IO;
			}
			if (status & ATA_SR_ERR) {
				ata_report(drv, write ? "WRITE SECTORS" : "READ SECTORS");
				return BLOCK_ERR_IO;
			}
		}

		if (write) {
			outsw(ch->io_base + ATA_REG_DATA, p, words);
		} else {
			insw(ch->io_base + ATA_REG_DATA, p, words);
		}
	}

	status = ide_wait_ready(ch, ATA_TIMEOUT_TICKS);
	if (status < 0) {
		klog(KLOG_WARN, "ata: transfer never finished on 0x%x %s\n", ch->io_base,
			 drv->index ? "slave" : "master");
		return BLOCK_ERR_IO;
	}

	if (status & ATA_SR_ERR) {
		// a media change mid-transfer (bit 3) means the drive silently
		// dropped sectors and the count we handed it is now a lie
		uint8_t error = ide_read_reg(ch, ATA_REG_ERROR);
		if (error & ATA_ER_MC) {
			klog(KLOG_WARN, "ata: media changed mid-transfer on 0x%x %s, results are garbage\n",
					ch->io_base, drv->index ? "slave" : "master");
		} else {
			ata_report(drv, write ? "WRITE SECTORS" : "READ SECTORS");
		}
		return BLOCK_ERR_IO;
	}

	return BLOCK_OK;
}

// tell the drive to push its write cache out to the platters. a drive that
// admits it has a volatile write cache is telling us our data is still in
// ram, so this is not optional after a write
static int ata_flush_cache(ide_drive_t* drv) {
	if (!drv->has_flush) {
		return BLOCK_OK;
	}

	ide_channel_t* ch = drv->channel;
	bool use_lba48 = drv->lba48;

	if (ide_wait_ready(ch, ATA_TIMEOUT_TICKS) < 0) {
		return BLOCK_ERR_IO;
	}

	if (use_lba48) {
		ata_hob(ch, true);

		ide_write_reg(ch, ATA_REG_FEATURES, 0x00);
		ide_write_reg(ch, ATA_REG_SECCOUNT, 0x00);
		ide_write_reg(ch, ATA_REG_LBA_MID, 0x00);
		ide_write_reg(ch, ATA_REG_LBA_HI, 0x00);
		ide_write_reg(ch, ATA_REG_DRIVE_HEAD, ata_head(drv));
		ata_issue(drv, ATA_CMD_FLUSH_CACHE_EXT);

		ata_hob(ch, false);
	} else {
		ide_write_reg(ch, ATA_REG_FEATURES, 0x00);
		ide_write_reg(ch, ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE);
	}

	// a flush can take a while on a cold platter, and it can be interrupted
	// by any number of writes the drive feels like finishing first
	int status = ide_wait_ready(ch, ATA_TIMEOUT_TICKS);
	if (status < 0) {
		klog(KLOG_WARN, "ata: FLUSH CACHE timed out on 0x%x %s\n", ch->io_base,
			 drv->index ? "slave" : "master");
		return BLOCK_ERR_IO;
	}

	if (status & ATA_SR_ERR) {
		ata_report(drv, "FLUSH CACHE");
		return BLOCK_ERR_IO;
	}

	return BLOCK_OK;
}

static int ata_rw(ide_drive_t* drv, uint64_t lba, void* buf, size_t count, bool write) {
	ide_channel_t* ch = drv->channel;

	if (lba + count > drv->block_count) {
		return BLOCK_ERR_INVAL;
	}

	// a chunk has to fit both the taskfile's 8 bit sector count and cx in
	// the rep insw that moves the data. on a 4096 byte advanced format drive
	// the rep is the tighter of the two
	size_t max_chunk = ((size_t)ATA_MAX_TRANSFER_WORDS * 2) / drv->sector_size;
	if (max_chunk == 0) {
		max_chunk = 1;
	}
	if (max_chunk > ATA_CHUNK_SECTORS) {
		max_chunk = ATA_CHUNK_SECTORS;
	}

	ide_channel_acquire(ch);

	int rc = BLOCK_OK;
	size_t done = 0;

	while (done < count) {
		size_t chunk = count - done;
		if (chunk > max_chunk) {
			chunk = max_chunk;
		}

		rc = ata_xfer_chunk(drv, lba + done, (uint8_t*)buf + done * drv->sector_size, chunk, write);
		if (rc != BLOCK_OK) {
			// the drive is in an unknown state after a failed command and
			// the next one may read garbage off it. reset it so the retry
			// starts from a known-good state
			ide_soft_reset(ch);
			break;
		}

		done += chunk;
	}

	// one flush for the lot rather than per chunk. the block layer has no
	// sync or close hook to batch this on, so correctness wins over speed
	if (rc == BLOCK_OK && write && drv->volatile_write_cache) {
		rc = ata_flush_cache(drv);
	}

	ide_channel_release(ch);
	return rc;
}

static int ata_read(void* impl, uint64_t block, void* buf, size_t count) {
	return ata_rw((ide_drive_t*)impl, block, buf, count, false);
}

static int ata_write(void* impl, uint64_t block, const void* buf, size_t count) {
	// the signature says const, the wire does not care
	return ata_rw((ide_drive_t*)impl, block, (void*)buf, count, true);
}

static uint64_t ata_count(void* impl) {
	return ((ide_drive_t*)impl)->block_count;
}

static const block_ops_t ata_ops = {
	.read = ata_read,
	.write = ata_write,
	.count = ata_count,
};

// a partition is just a window onto its parent device: same block ops, an
// offset baked in. stacking one block_register on another is all it takes
typedef struct {
	block_dev_t* parent;
	uint64_t start_lba;
	uint64_t block_count;
} ata_partition_t;

static int part_read(void* impl, uint64_t block, void* buf, size_t count) {
	ata_partition_t* part = (ata_partition_t*)impl;
	if (block + count > part->block_count) {
		return BLOCK_ERR_INVAL;
	}
	return part->parent->ops->read(part->parent->impl, part->start_lba + block, buf, count);
}

static int part_write(void* impl, uint64_t block, const void* buf, size_t count) {
	ata_partition_t* part = (ata_partition_t*)impl;
	if (block + count > part->block_count) {
		return BLOCK_ERR_INVAL;
	}
	return part->parent->ops->write(part->parent->impl, part->start_lba + block, buf, count);
}

static uint64_t part_count(void* impl) {
	return ((ata_partition_t*)impl)->block_count;
}

static const block_ops_t part_ops = {
	.read = part_read,
	.write = part_write,
	.count = part_count,
};

// walk the mbr and publish each partition as /dvc/diskNp<n>. no extended
// partition chains, no gpt: a linear read of the four primary entries is
// enough to boot a fat drive
static void ata_scan_partitions(block_dev_t* disk, const char* disk_path) {
	uint8_t mbr[MBR_SIZE];

	if (disk->ops->read(disk->impl, 0, mbr, 1) != BLOCK_OK) {
		return;
	}

	uint16_t signature = (uint16_t)mbr[510] | ((uint16_t)mbr[511] << 8);
	if (signature != MBR_SIGNATURE) {
		return; // no partition table, could be a superfloppy
	}

	size_t dlen = strlen(disk_path);

	for (int i = 0; i < MBR_MAX_PARTITIONS; i++) {
		const uint8_t* entry = mbr + MBR_TABLE_OFFSET + i * MBR_ENTRY_SIZE;

		uint8_t status = entry[0];
		uint8_t type = entry[4];
		uint32_t start = read_le32(entry + 8);
		uint32_t count = read_le32(entry + 12);

		if (type == MBR_TYPE_EMPTY || type == MBR_TYPE_GPT_PROTECTIVE) {
			continue;
		}

		// 0x00 for a plain primary, 0x80 for a bootable one
		if (status != 0x00 && status != 0x80) {
			continue;
		}

		if (count == 0) {
			continue;
		}

		// a lying partition table would hand isofs or fatfs a window off the
		// end of the disk, and they would read whatever is there instead
		if ((uint64_t)start + count > disk->block_count) {
			klog(KLOG_WARN, "ata: %s p%d claims sectors %u..%u, past the end of the disk\n",
					disk_path, i + 1, start, (uint32_t)((uint64_t)start + count));
			continue;
		}

		ata_partition_t* part = kmalloc(sizeof(ata_partition_t), alignof(ata_partition_t));
		if (!part) {
			klog(KLOG_WARN, "ata: kmalloc failed for %s p%d\n", disk_path, i + 1);
			return;
		}

		part->parent = disk;
		part->start_lba = start;
		part->block_count = count;

		char path[24];
		if (dlen + 3 >= sizeof(path)) {
			kfree(part);
			return;
		}

		memcpy(path, disk_path, dlen);
		path[dlen] = 'p';
		path[dlen + 1] = (char)('1' + i);
		path[dlen + 2] = '\0';

		if (!block_register(path, &part_ops, part, disk->block_size, count)) {
			kfree(part);
			return;
		}

		klog(KLOG_INFO, "ata: %s type 0x%02x, %u sectors at lba %u\n", path, type, count, start);
	}
}

static int next_disk_number = 0;

static void ata_attach(ide_drive_t* drv) {
	uint16_t words[256];

	if (ide_identify(drv, words) != 0) {
		klog(KLOG_WARN, "ata: IDENTIFY failed, skipping 0x%x %s\n", drv->channel->io_base,
			 drv->index ? "slave" : "master");
		return;
	}

	ata_apply_identify(drv, words);

	if (drv->block_count == 0) {
		klog(KLOG_WARN, "ata: drive reports no sectors, skipping\n");
		return;
	}

	char model[41];
	ata_string(words, 27, 20, model, sizeof(model));

	klog(KLOG_INFO, "ata: %s, %u sectors x %u bytes, lba48=%s flush=%s\n",
		 model, (uint32_t)drv->block_count, drv->sector_size,
		 drv->lba48 ? "yes" : "no", drv->has_flush ? "yes" : "no");

	int num = next_disk_number++;
	if (num > 9) {
		klog(KLOG_WARN, "ata: more than ten disks? really?\n");
	}

	char path[16];
	path[0] = '/';
	path[1] = 'd';
	path[2] = 'v';
	path[3] = 'c';
	path[4] = '/';
	path[5] = 'd';
	path[6] = 'i';
	path[7] = 's';
	path[8] = 'k';
	path[9] = (char)('0' + num);
	path[10] = '\0';

	block_dev_t* disk = block_register(path, &ata_ops, drv, drv->sector_size, drv->block_count);
	if (!disk) {
		klog(KLOG_WARN, "ata: could not register %s\n", path);
		return;
	}

	klog(KLOG_INFO, "ata: %s registered (%llu sectors)\n", path, (unsigned long long)drv->block_count);

	ata_scan_partitions(disk, path);
}

void ata_init(void) {
	for (int bus = 0; bus < IDE_CHANNEL_COUNT; bus++) {
		ide_channel_t* ch = ide_channel_get(bus);
		if (!ch) continue;

		for (int slot = 0; slot < IDE_SLAVES_PER_CHANNEL; slot++) {
			ide_drive_t* drv = &ch->drives[slot];
			if (!drv->present || drv->is_atapi) continue;
			ata_attach(drv);
		}
	}
}
