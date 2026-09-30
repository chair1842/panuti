/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ide.h"
#include <stdint.h>
#include <stddef.h>
#include "../../io.h"
#include <kernel/ata/atapi.h>
#include <kernel/block/block.h>
#include <kernel/klog.h>
#include <string.h>

#define ATAPI_SECTOR_SIZE 2048

#define SCSI_READ_CAPACITY 0x25
#define SCSI_READ10 0x28

// each registered cdrom gets the next free number, no matter which
// slot on which bus it squats in
static int next_cdrom_number = 0;

// the atapi secret sauce: shove a scsi command packet down the drive's throat
// and hope the data pops out the other end. transfer_bytes tells the drive
// how much data to shove back (a whole sector, or just an 8-byte read
// capacity reply. size matters).
//
// pio-polling version: we wait for the drive to assert DRQ ourselves instead
// of betting on irq timing. with irqs the data can land in the buffer one
// transfer late (the drive raises intq for the packet phase too), which
// scrambles which sector we actually read on the next go.
static int ide_send_packet_locked(
	ide_drive_t* drv,
	const uint8_t cdb[12],
	void* buf,
	size_t transfer_bytes
) {
	ide_channel_t* ch = drv->channel;

	if (ide_poll(ch) < 0) {
		return BLOCK_ERR_IO;
	}

	// features = 0, no interrupts we dont care
	ide_write_reg(ch, ATA_REG_FEATURES, 0);

	// byte count for the transfer, little-endian over two registers
	uint16_t byte_count = (uint16_t)transfer_bytes;
	ide_write_reg(ch, ATA_REG_LBA_MID, byte_count & 0xFF);
	ide_write_reg(ch, ATA_REG_LBA_HI, byte_count >> 8);

	// pick master or slave, then send the packet command
	ide_write_reg(ch, ATA_REG_DRIVE_HEAD, drv->index ? ATA_HEAD_SLAVE : ATA_HEAD_MASTER);
	ide_write_reg(ch, ATA_REG_COMMAND, ATA_CMD_PACKET);

	// wait for the drive to accept the packet (drq set, bsy clear)
	int status = ide_wait_drq(ch, IDE_TIMEOUT_TICKS);
	if (status < 0) {
		return BLOCK_ERR_IO;
	}

	if (status & ATA_SR_ERR) {
		klog(KLOG_WARN, "atapi: PACKET aborted on 0x%x %s: %s\n",
			 ch->io_base, drv->index ? "slave" : "master",
			 ide_error_string(ide_read_reg(ch, ATA_REG_ERROR)));
		return BLOCK_ERR_IO;
	}

	// the cdb is 12 bytes = 6 words. outsw/insw are word-based.
	outsw(ch->io_base + ATA_REG_DATA, cdb, 6);

	// wait for the data to be ready for pio-out: bsy clear and drq set again
	status = ide_wait_drq(ch, IDE_TIMEOUT_TICKS);
	if (status < 0) {
		return BLOCK_ERR_IO;
	}

	if (status & ATA_SR_ERR) {
		klog(KLOG_WARN, "atapi: transfer failed on 0x%x %s: %s\n",
			 ch->io_base, drv->index ? "slave" : "master",
			 ide_error_string(ide_read_reg(ch, ATA_REG_ERROR)));
		return BLOCK_ERR_IO;
	}

	// bytes of data are words of nothing, delivered straight to your door
	insw(ch->io_base + ATA_REG_DATA, buf, transfer_bytes / 2);

	// let the drive finish the transfer before we send the next packet
	if (ide_wait_ready(ch, IDE_TIMEOUT_TICKS) < 0) {
		return BLOCK_ERR_IO;
	}

	return BLOCK_OK;
}

static int ide_send_packet(ide_drive_t* drv, const uint8_t cdb[12], void* buf, size_t transfer_bytes) {
	ide_channel_acquire(drv->channel);
	int rc = ide_send_packet_locked(drv, cdb, buf, transfer_bytes);
	ide_channel_release(drv->channel);
	return rc;
}

static inline uint32_t read_be32(const uint8_t* p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}

static void atapi_read_capacity(ide_drive_t* drv, uint32_t* sectors) {
	uint8_t cdb[12] = { SCSI_READ_CAPACITY };
	uint8_t res[8];
	if (ide_send_packet(drv, cdb, res, 8) != BLOCK_OK) {
		*sectors = 0;
		return;
	}
	
	// bytes 0-3 = last lba, sectors = last + 1
	*sectors = read_be32(res) + 1;
}

static int atapi_read(void* impl, uint64_t block, void* buf, size_t count) {
	ide_drive_t* drv = (ide_drive_t*)impl;
	uint8_t* out = (uint8_t*)buf;

	for (size_t i = 0; i < count; i++) {
		uint8_t cdb[12] = { 0 };
		cdb[0] = SCSI_READ10;
		cdb[2] = (block >> 24) & 0xFF;
		cdb[3] = (block >> 16) & 0xFF;
		cdb[4] = (block >> 8) & 0xFF;
		cdb[5] = block & 0xFF;
		cdb[8] = 1; // 1 sector per packet

		if (ide_send_packet(drv, cdb, out, ATAPI_SECTOR_SIZE) != BLOCK_OK) {
			return BLOCK_ERR_IO;
		}

		out += ATAPI_SECTOR_SIZE;
		block++;
	}

	return BLOCK_OK;
}

static int atapi_write(void* impl, uint64_t block, const void* buf, size_t count) {
	// cdroms burn, not write
	(void)impl; (void)block; (void)buf; (void)count;
	return BLOCK_ERR_IO;
}

static uint64_t atapi_count(void* impl) {
	ide_drive_t* drv = (ide_drive_t*)impl;
	return drv->block_count;
}

static const block_ops_t atapi_ops = {
	.read = atapi_read,
	.write = atapi_write,
	.count = atapi_count,
};

static void atapi_attach(ide_drive_t* drv) {
	uint32_t sectors;
	atapi_read_capacity(drv, &sectors);
	drv->block_count = sectors;

	char path[16];
	const char* prefix = "/dvc/cdrom";
	size_t plen = strlen(prefix);

	// numbers never get reused; cdrom0 is simply whoever shows up first
	int num = next_cdrom_number++;
	if (num > 9) {
		klog(KLOG_WARN, "atapi: more than ten cdroms? really?\n");
	}

	memcpy(path, prefix, plen + 1);
	path[plen] = '0' + num;
	path[plen + 1] = '\0';
	block_register(path, &atapi_ops, drv, ATAPI_SECTOR_SIZE, sectors);
	klog(KLOG_INFO, "atapi: %s registered (%u sectors)\n", path, sectors);
}

void atapi_init(void) {
	for (int bus = 0; bus < IDE_CHANNEL_COUNT; bus++) {
		ide_channel_t* ch = ide_channel_get(bus);
		if (!ch) continue;

		for (int slot = 0; slot < IDE_SLAVES_PER_CHANNEL; slot++) {
			ide_drive_t* drv = &ch->drives[slot];
			if (!drv->present || !drv->is_atapi) continue;
			atapi_attach(drv);
		}
	}
}
