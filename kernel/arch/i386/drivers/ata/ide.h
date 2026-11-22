/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef ARCH_I386_DRIVERS_ATA_IDE_H
#define ARCH_I386_DRIVERS_ATA_IDE_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/sched/task.h>
#include <kernel/sched/wait.h>

// uwu contants : 3

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
#define ATA_CMD_READ_SECTORS_EXT 0x24
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_WRITE_SECTORS_EXT 0x34
#define ATA_CMD_PACKET 0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_FLUSH_CACHE 0xE7
#define ATA_CMD_FLUSH_CACHE_EXT 0xEA
#define ATA_CMD_SET_FEATURES 0xEF

#define ATA_CMD_READ_DMA 0xC8
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA 0xCA
#define ATA_CMD_WRITE_DMA_EXT 0x35

#define ATA_FEATURE_PACKET_DMA 0x01

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

#define IDE_BM_CMD 0x00
#define IDE_BM_STATUS 0x02
#define IDE_BM_PRDT 0x04

#define IDE_BM_CMD_START 0x01
#define IDE_BM_CMD_READ 0x08

#define IDE_BM_STATUS_ACTIVE 0x01
#define IDE_BM_STATUS_ERROR 0x02
#define IDE_BM_STATUS_IRQ 0x04
#define IDE_BM_STATUS_TIMING 0x60

#define IDE_PRD_EOT 0x8000
#define IDE_PRD_ENTRIES 512

#define IDE_DMA_TIMEOUT (-1)
#define IDE_DMA_KILLED (-2)

typedef struct ide_channel ide_channel_t;
typedef struct ide_drive ide_drive_t;

typedef struct ide_prd {
	uint32_t addr;
	uint16_t count;
	uint16_t flags;
} __attribute__((packed)) ide_prd_t;

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
	bool dma_ok;
};

struct ide_channel {
	uint16_t io_base;
	uint16_t ctrl_base;
	ide_drive_t drives[IDE_SLAVES_PER_CHANNEL];
	volatile bool irq_fired;

	uint16_t bm_base;
	bool dma_ok;
	ide_prd_t* prd;
	uint32_t prd_phys;
	void* bounce;
	uint32_t bounce_size;

	volatile bool dma_done;
	volatile uint8_t dma_status;
	volatile uint8_t dma_ata_status;
	task_t* dma_waiter;
	uint8_t control;

	bool locked;
	wait_queue_t lock_waiters;
};

uint8_t ide_read_reg(ide_channel_t* ch, uint8_t offset);
void ide_write_reg(ide_channel_t* ch, uint8_t offset, uint8_t val);

int ide_poll(ide_channel_t* ch);
void ide_wait_irq(ide_channel_t* ch);

ide_channel_t* ide_channel_get(int bus);
int ide_probe_channel(ide_channel_t* ch);
void ide_soft_reset(ide_channel_t* ch);

int ide_dma_init(void);

bool ide_dma_needs_bounce(const void* buf, size_t bytes);
int ide_dma_prepare(ide_channel_t* ch, void* buf, size_t bytes);
void ide_dma_start(ide_channel_t* ch, bool write);
int ide_dma_wait(ide_channel_t* ch, uint32_t ticks);
void ide_dma_stop(ide_channel_t* ch);

int ide_wait_ready(ide_channel_t* ch, uint32_t ticks);

int ide_wait_drq(ide_channel_t* ch, uint32_t ticks);

void ide_channel_acquire(ide_channel_t* ch);
void ide_channel_release(ide_channel_t* ch);

int ide_identify(ide_drive_t* drv, uint16_t* words);
int ide_identify_packet(ide_drive_t* drv, uint16_t* words);

const char* ide_error_string(uint8_t error);

void ide_control_write(ide_channel_t* ch, uint8_t value);
void ide_control_update(ide_channel_t* ch, uint8_t set, uint8_t clear);

void ide_dma_arm(ide_channel_t* ch);

#endif
