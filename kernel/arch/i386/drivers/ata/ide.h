#ifndef ARCH_I386_DRIVERS_ATA_IDE_H
#define ARCH_I386_DRIVERS_ATA_IDE_H

#include <stdint.h>
#include <stdbool.h>

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
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_PACKET 0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6
#define ATA_SECONDARY_IO 0x170
#define ATA_SECONDARY_CTRL 0x376

#define ATA_HEAD_MASTER 0xA0
#define ATA_HEAD_SLAVE 0xB0

#define IDE_TIMEOUT_TICKS 500

typedef struct ide_channel {
	uint16_t io_base;
	uint16_t ctrl_base;
	
	bool is_slave;
	bool present;
	bool is_atapi;
	uint64_t block_count;
	
	volatile bool irq_fired;
} ide_channel_t;

uint8_t ide_read_reg(ide_channel_t* ch, uint8_t offset);
void ide_write_reg(ide_channel_t* ch, uint8_t offset, uint8_t val);
int ide_poll(ide_channel_t* ch);
int ide_probe(ide_channel_t* ch);
void ide_wait_irq(ide_channel_t* ch);

#endif