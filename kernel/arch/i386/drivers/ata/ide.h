/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef ARCH_I386_DRIVERS_ATA_IDE_H
#define ARCH_I386_DRIVERS_ATA_IDE_H

#include <stdint.h>
#include <stddef.h>

#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECCOUNT 0x02
#define ATA_REG_LBA_LO 0x03
#define ATA_REG_LBA_MID 0x04
#define ATA_REG_LBA_HI 0x05
#define ATA_REG_DRIVE_HEAD 0x06
#define ATA_REG_STATUS 0x07
#define ATA_REG_COMMAND 0x07

#define ATA_REG_ALT_STATUS 0x00
#define ATA_REG_DEV_CTRL 0x00

#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF 0x20
#define ATA_SR_DSC 0x10
#define ATA_SR_DRQ 0x08
#define ATA_SR_CORR 0x04
#define ATA_SR_IDX 0x02
#define ATA_SR_ERR 0x01

#define ATA_ER_AMNF 0x01 // address mark not found
#define ATA_ER_TK0NF 0x02 // track 0 not found
#define ATA_ER_ABRT 0x04 // command aborted
#define ATA_ER_MCR 0x08 // media change requested
#define ATA_ER_IDNF 0x10 // ID not found
#define ATA_ER_MC 0x20 // media changed
#define ATA_ER_UNC 0x40 // unrecoverable read/write error
#define ATA_ER_BBK 0x80 // bad block

#define ATA_CMD_READ_SECTORS 0x20
// 0x25/0x35 are the ACS-4 READ/WRITE SECTORS EXT opcodes real drives expect.
// note qemu 11.1 is unusable for exercising them: hw/ide/ide-internal.h names
// 0x24 WIN_READ_EXT and 0x34 WIN_WRITE_EXT, so its ide_cmd_table hands our 0x25
// to cmd_read_dma and a pio data phase never happens. patch qemu or test the
// 48 bit path on hardware.
#define ATA_CMD_READ_SECTORS_EXT 0x25
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_WRITE_SECTORS_EXT 0x35
#define ATA_CMD_PACKET 0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_FLUSH_CACHE 0xE7
#define ATA_CMD_FLUSH_CACHE_EXT 0xEA
#define ATA_CMD_SET_FEATURES 0xEF

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6
#define ATA_SECONDARY_IO 0x170
#define ATA_SECONDARY_CTRL 0x376

#define ATA_DCR_HOB 0x80
#define ATA_DCR_SRST 0x04
#define ATA_DCR_NIEN 0x02

#define ATA_HEAD_MASTER 0xA0
#define ATA_HEAD_SLAVE 0xB0
#define ATA_HEAD_LBA 0x40

#define IDE_CHANNEL_COUNT 2
#define IDE_SLAVES_PER_CHANNEL 2

#define IDE_TIMEOUT_TICKS 500
#define ATA_TIMEOUT_TICKS 3000

typedef struct ide_channel ide_channel_t;
typedef struct ide_drive ide_drive_t;

struct ide_drive {
	ide_channel_t* channel;
	uint8_t index; // 0 = master, 1 = slave
	bool present;
	bool is_atapi;
	bool lba48;
	bool has_flush;
	bool volatile_write_cache;
	uint32_t sector_size;
	uint64_t block_count;
};

struct ide_channel {
	uint16_t io_base;
	uint16_t ctrl_base;
	ide_drive_t drives[IDE_SLAVES_PER_CHANNEL];
	volatile bool irq_fired;

	// set while a task owns the channel's registers; see ide_channel_acquire
	volatile uint32_t busy;
};

uint8_t ide_read_reg(ide_channel_t* ch, uint8_t offset);
void ide_write_reg(ide_channel_t* ch, uint8_t offset, uint8_t val);

int ide_poll(ide_channel_t* ch);
void ide_wait_irq(ide_channel_t* ch);

int ide_init(void);
ide_channel_t* ide_channel_get(int bus);
int ide_probe_channel(ide_channel_t* ch);
void ide_soft_reset(ide_channel_t* ch);

int ide_wait_ready(ide_channel_t* ch, uint32_t ticks);

int ide_wait_drq(ide_channel_t* ch, uint32_t ticks);

void ide_channel_acquire(ide_channel_t* ch);
void ide_channel_release(ide_channel_t* ch);

int ide_identify(ide_drive_t* drv, uint16_t* words);

const char* ide_error_string(uint8_t error);

#endif
