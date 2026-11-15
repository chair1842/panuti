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

#define ATAPI_DMA_MAX_SECTORS 256

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

static int ide_send_packet_dma_locked(ide_drive_t* drv, const uint8_t cdb[12], void* buf, size_t transfer_bytes) {
	ide_channel_t* ch = drv->channel;

	if ((transfer_bytes % ATAPI_SECTOR_SIZE) != 0 || ((uintptr_t)buf & (ATAPI_SECTOR_SIZE - 1)) != 0) {
		klog(
			KLOG_WARN,
			"atapi: DMA transfer of %zu bytes at %p is not sector aligned\n",
			transfer_bytes,
			buf
		);
		
		return BLOCK_ERR_IO;
	}

	if (ide_poll(ch) < 0) {
		return BLOCK_ERR_IO;
	}

	if (ide_dma_prepare(ch, buf, transfer_bytes) != 0) {
		klog(
			KLOG_WARN,
			"atapi: cannot describe %zu bytes for DMA on 0x%x\n",
			transfer_bytes,
			ch->io_base
		);
		
		return BLOCK_ERR_IO;
	}

	ide_write_reg(ch, ATA_REG_FEATURES, ATA_FEATURE_PACKET_DMA);

	uint16_t byte_count = (uint16_t)transfer_bytes;
	ide_write_reg(ch, ATA_REG_LBA_MID, byte_count & 0xFF);
	ide_write_reg(ch, ATA_REG_LBA_HI, byte_count >> 8);

	ide_write_reg(ch, ATA_REG_DRIVE_HEAD, drv->index ? ATA_HEAD_SLAVE : ATA_HEAD_MASTER);
	ide_write_reg(ch, ATA_REG_COMMAND, ATA_CMD_PACKET);

	int status = ide_wait_drq(ch, IDE_TIMEOUT_TICKS);
	if (status < 0) {
		return BLOCK_ERR_IO;
	}

	if (status & ATA_SR_ERR) {
		klog(
			KLOG_WARN,
			"atapi: PACKET aborted on 0x%x %s: %s\n",
			ch->io_base,
			drv->index ? "slave" : "master",
			ide_error_string(ide_read_reg(ch, ATA_REG_ERROR))
		);
		
		return BLOCK_ERR_IO;
	}

	outsw(ch->io_base + ATA_REG_DATA, cdb, 6);

	ide_read_reg(ch, ATA_REG_STATUS);

	uint8_t prev_control = ch->control;
	ide_dma_arm(ch);
	ide_control_update(ch, 0, ATA_DCR_NIEN);

	ide_dma_start(ch, false);

	int wait = ide_dma_wait(ch, ATA_TIMEOUT_TICKS);

	ide_dma_stop(ch);
	ide_control_write(ch, prev_control);

	if (wait < 0) {
		klog(
			KLOG_WARN,
			"atapi: DMA read %s on 0x%x %s\n",
			wait == IDE_DMA_KILLED ? "interrupted" : "stalled",
			ch->io_base,
			drv->index ? "slave" : "master"
		);

		return BLOCK_ERR_IO;
	}

	if (ch->dma_status & IDE_BM_STATUS_ERROR) {
		klog(
			KLOG_WARN,
			"atapi: bus master DMA error on 0x%x %s\n",
			ch->io_base,
			drv->index ? "slave" : "master"
		);

		return BLOCK_ERR_IO;
	}

	status = ide_wait_ready(ch, ATA_TIMEOUT_TICKS);
	if (status < 0) {
		klog(
			KLOG_WARN,
			"atapi: DMA transfer never finished on 0x%x %s\n",
			ch->io_base,
			drv->index ? "slave" : "master"
		);
		
		return BLOCK_ERR_IO;
	}

	if (status & ATA_SR_ERR) {
		klog(
			KLOG_WARN,
			"atapi: DMA transfer failed on 0x%x %s: %s\n",
			ch->io_base,
			drv->index ? "slave" : "master",
			ide_error_string(ide_read_reg(ch, ATA_REG_ERROR))
		);

		return BLOCK_ERR_IO;
	}

	return BLOCK_OK;
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
	ide_channel_t* ch = drv->channel;
	uint8_t* out = (uint8_t*)buf;

	bool dma = drv->dma_ok && ch->dma_ok && ch->bounce && ch->bounce_size >= ATAPI_SECTOR_SIZE;

	size_t max_chunk = 1;

	if (dma) {
		max_chunk = ch->bounce_size / ATAPI_SECTOR_SIZE;

		if (max_chunk > ATAPI_DMA_MAX_SECTORS) {
			max_chunk = ATAPI_DMA_MAX_SECTORS;
		}

		if (max_chunk == 0) {
			dma = false;
			max_chunk = 1;
		}
	}

	ide_channel_acquire(ch);

	int rc = BLOCK_OK;
	size_t done = 0;

	while (done < count) {
		size_t chunk = count - done;
		if (chunk > max_chunk) {
			chunk = max_chunk;
		}

		uint32_t lba = (uint32_t)(block + done);

		uint8_t cdb[12] = { 0 };
		cdb[0] = SCSI_READ10;
		cdb[2] = (lba >> 24) & 0xFF;
		cdb[3] = (lba >> 16) & 0xFF;
		cdb[4] = (lba >> 8) & 0xFF;
		cdb[5] = lba & 0xFF;
		cdb[7] = (chunk >> 8) & 0xFF;
		cdb[8] = chunk & 0xFF;

		if (dma) {
			size_t bytes = chunk * ATAPI_SECTOR_SIZE;

			rc = ide_send_packet_dma_locked(drv, cdb, ch->bounce, bytes);
			if (rc == BLOCK_OK) {
				memcpy(out + done * ATAPI_SECTOR_SIZE, ch->bounce, bytes);
			}
		} else {
			rc = ide_send_packet_locked(drv, cdb, out + done * ATAPI_SECTOR_SIZE, ATAPI_SECTOR_SIZE);
		}

		if (rc != BLOCK_OK) {
			if (dma) {
				ide_dma_stop(ch);
				ide_soft_reset(ch);
			}
			
			break;
		}

		done += chunk;
	}

	ide_channel_release(ch);
	return rc;
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
	uint16_t words[256];

	if (ide_identify_packet(drv, words) == 0) {
		drv->dma_ok = drv->channel->dma_ok && (words[49] & 0x0100) != 0;
	} else {
		klog(KLOG_WARN, "atapi: IDENTIFY PACKET failed on 0x%x %s, DMA disabled\n",
			drv->channel->io_base,
			drv->index ? "slave" : "master"
		);
		
		drv->dma_ok = false;
	}

	uint32_t sectors;
	atapi_read_capacity(drv, &sectors);
	drv->block_count = sectors;

	char path[16];
	const char* prefix = "/dvc/cdrom/";
	size_t plen = strlen(prefix);

	// numbers never get reused; cdrom/0 is simply whoever shows up first
	int num = next_cdrom_number++;
	if (num > 9) {
		klog(KLOG_WARN, "atapi: more than ten cdroms? really?\n");
	}

	memcpy(path, prefix, plen + 1);
	path[plen] = '0' + num;
	path[plen + 1] = '\0';
	block_register(path, &atapi_ops, drv, ATAPI_SECTOR_SIZE, sectors);
	klog(KLOG_INFO, "atapi: %s registered (%u sectors, dma=%s)\n", path, sectors, drv->dma_ok ? "yes" : "no");
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
