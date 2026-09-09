#include "ide.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "../../io.h"
#include "../../intpt/handlers/main.h"
#include <kernel/ata/atapi.h>
#include <kernel/block/block.h>
#include <kernel/timer.h>
#include <kernel/klog.h>
#include <string.h>

#define ATAPI_SECTOR_SIZE 2048

#define SCSI_READ_CAPACITY 0x25
#define SCSI_READ10 0x28

#define ATA_IRQ14_VECTOR 46
#define ATA_IRQ15_VECTOR 47

static ide_channel_t primary = {
	.io_base = ATA_PRIMARY_IO,
	.ctrl_base = ATA_PRIMARY_CTRL,
};

static ide_channel_t secondary = {
	.io_base = ATA_SECONDARY_IO,
	.ctrl_base = ATA_SECONDARY_CTRL,
};

static void ata_irq14_handler(registers_t* regs) {
	(void)regs;
	primary.irq_fired = true;
	ide_read_reg(&primary, ATA_REG_STATUS);
}

static void ata_irq15_handler(registers_t* regs) {
	(void)regs;
	secondary.irq_fired = true;
	ide_read_reg(&secondary, ATA_REG_STATUS);
}

// the atapi secret sauce: shove a scsi command packet down the drive's throat
// and hope the data pops out the other end. transfer_bytes tells the drive
// how much data to shove back (a whole sector, or just an 8-byte read
// capacity reply. size matters).
//
// pio-polling version: we wait for the drive to assert DRQ ourselves instead
// of betting on irq timing. with irqs the data can land in the buffer one
// transfer late (the drive raises intq for the packet phase too), which
// scrambles which sector we actually read on the next go.
static int ide_send_packet(
	ide_channel_t* ch,
	const uint8_t cdb[12],
	void* buf,
	size_t transfer_bytes
) {
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
	ide_write_reg(ch, ATA_REG_DRIVE_HEAD, ch->is_slave ? ATA_HEAD_SLAVE : ATA_HEAD_MASTER);
	ide_write_reg(ch, ATA_REG_COMMAND, ATA_CMD_PACKET);

	// wait for the drive to accept the packet (drq set, bsy clear)
	uint32_t timeout = timer_get_ticks() + IDE_TIMEOUT_TICKS;
	uint8_t status;
	do {
		// sleep instead of spinning; the drive's intq (or the timer) wakes us
		__asm__ __volatile__("hlt");
		status = ide_read_reg(ch, ATA_REG_STATUS);
		if (timer_get_ticks() > timeout) {
			return BLOCK_ERR_IO;
		}
	} while ((status & ATA_SR_BSY) || !(status & ATA_SR_DRQ));

	// the cdb is 12 bytes = 6 words. outsw/insw are word-based.
	outsw(ch->io_base + ATA_REG_DATA, cdb, 6);

	// wait for the data to be ready for pio-out: bsy clear and drq set again
	timeout = timer_get_ticks() + IDE_TIMEOUT_TICKS;
	do {
		__asm__ __volatile__("hlt");
		status = ide_read_reg(ch, ATA_REG_STATUS);
		if (timer_get_ticks() > timeout) {
			return BLOCK_ERR_IO;
		}
	} while ((status & ATA_SR_BSY) || !(status & ATA_SR_DRQ));

	// bytes of data are words of nothing, delivered straight to your door
	insw(ch->io_base + ATA_REG_DATA, buf, transfer_bytes / 2);

	// let the drive finish the transfer before we send the next packet
	timeout = timer_get_ticks() + IDE_TIMEOUT_TICKS;
	do {
		__asm__ __volatile__("hlt");
		status = ide_read_reg(ch, ATA_REG_STATUS);
		if (timer_get_ticks() > timeout) {
			return BLOCK_ERR_IO;
		}
	} while (status & ATA_SR_BSY);

	return BLOCK_OK;
}

static inline uint32_t read_be32(const uint8_t* p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}

static void atapi_read_capacity(ide_channel_t* ch, uint32_t* sectors) {
	uint8_t cdb[12] = { SCSI_READ_CAPACITY };
	uint8_t res[8];
	if (ide_send_packet(ch, cdb, res, 8) != BLOCK_OK) {
		*sectors = 0;
		return;
	}
	
	// bytes 0-3 = last lba, sectors = last + 1
	*sectors = read_be32(res) + 1;
}

static int atapi_read(void* impl, uint64_t block, void* buf, size_t count) {
	ide_channel_t* ch = (ide_channel_t*)impl;
	uint8_t* out = (uint8_t*)buf;

	for (size_t i = 0; i < count; i++) {
		uint8_t cdb[12] = { 0 };
		cdb[0] = SCSI_READ10;
		cdb[2] = (block >> 24) & 0xFF;
		cdb[3] = (block >> 16) & 0xFF;
		cdb[4] = (block >> 8) & 0xFF;
		cdb[5] = block & 0xFF;
		cdb[8] = 1; // 1 sector per packet

		if (ide_send_packet(ch, cdb, out, ATAPI_SECTOR_SIZE) != BLOCK_OK) {
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
	ide_channel_t* ch = (ide_channel_t*)impl;
	return ch->block_count;
}

// each registered cdrom gets the next free number, no matter which
// slot on which bus it squats in
static int next_cdrom_number = 0;

static const block_ops_t atapi_ops = {
	.read = atapi_read,
	.write = atapi_write,
	.count = atapi_count,
};

static void atapi_scan_channel(ide_channel_t* ch, int bus) {
	if (ide_probe(ch) != 0) {
		return;
	}

	if (!ch->is_atapi) {
		klog(KLOG_INFO, "atapi: ata disk on %s%s not supported yet\n",
			 bus ? "2nd" : "1st", ch->is_slave ? " slave" : " master");
		
		return;
	}

	uint32_t sectors;
	atapi_read_capacity(ch, &sectors);
	ch->block_count = sectors;

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
	block_register(path, &atapi_ops, ch, ATAPI_SECTOR_SIZE, sectors);
	klog(KLOG_INFO, "atapi: %s registered (%u sectors)\n", path, sectors);
}

void atapi_init(void) {
	register_handler(ATA_IRQ14_VECTOR, ata_irq14_handler);
	register_handler(ATA_IRQ15_VECTOR, ata_irq15_handler);

	atapi_scan_channel(&primary, 0);
	atapi_scan_channel(&secondary, 1);
}