#include "ide.h"
#include <stdbool.h>
#include <stdint.h>
#include "../../io.h"
#include <kernel/timer.h>
#include <kernel/klog.h>

#define IDE_TIMEOUT_TICKS 500

#define ATA_HEAD_MASTER 0xA0
#define ATA_HEAD_SLAVE 0xB0

uint8_t ide_read_reg(ide_channel_t* ch, uint8_t offset) {
	return inb(ch->io_base + offset);
}

void ide_write_reg(ide_channel_t* ch, uint8_t offset, uint8_t val) {
	outb(ch->io_base + offset, val);
}

int ide_poll(ide_channel_t* ch) {
	// instead of writing iowait 4 times, i write this. oh the bliss of code styling.
	// i know that with an unoptimized compilation this would be slow as hell
	// but yay we have optimization
	for (volatile int i = 0; i < 4; i++) {
		iowait();
	}

	uint32_t timeout = timer_get_ticks() + IDE_TIMEOUT_TICKS;
	uint8_t status;

	// if ur confused about this, dont worry, i consulted the ai
	// it will run the do thing at least once before checking the condition
	// this syntax is so fucking unintuitive

	do {
		status = ide_read_reg(ch, ATA_REG_STATUS);
		if (timer_get_ticks() > timeout) {
			return -1;
		}
	} while (status & ATA_SR_BSY);

	return status;
}

void ide_wait_irq(ide_channel_t* ch) {
	uint32_t timeout = timer_get_ticks() + IDE_TIMEOUT_TICKS;

	while (!ch->irq_fired) {
		if (timer_get_ticks() > timeout) {
			klog(KLOG_WARN, "ide: IRQ timeout\n");
			return;
		}

		__asm__ __volatile__("hlt");
	}

	ch->irq_fired = false;
	ide_read_reg(ch, ATA_REG_STATUS);
}

int ide_probe(ide_channel_t* ch) {
	ch->present = false;
	ch->is_atapi = false;

	outb(ch->ctrl_base + ATA_REG_DEV_CTRL, 0x04);
	
	for (volatile int i = 0; i < 1000000; i++) {
		iowait();
	}
	
	outb(ch->ctrl_base + ATA_REG_DEV_CTRL, 0x00);

	// this will check master then slave
	for (int drive = 0; drive < 2; drive++) {
		// because 0 is master and 1 is slave
		ch->is_slave = drive;

		uint8_t head = drive ? ATA_HEAD_SLAVE : ATA_HEAD_MASTER;

		ide_write_reg(ch, ATA_REG_DRIVE_HEAD, head);
		ide_write_reg(ch, ATA_REG_SECCOUNT, 0);
		ide_write_reg(ch, ATA_REG_LBA_LO, 0);
		ide_write_reg(ch, ATA_REG_LBA_MID, 0);
		ide_write_reg(ch, ATA_REG_LBA_HI, 0);
		ide_write_reg(ch, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
		
		uint8_t status = ide_read_reg(ch, ATA_REG_STATUS);
		if (status == 0) {
			continue;
		}

		status = ide_poll(ch);
		if (status < 0) {
			continue;
		}

		if (status & ATA_SR_ERR) {
			ide_write_reg(ch, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			status = ide_poll(ch);
			if (status < 0) {
				continue;
			}
		}
		
		uint16_t ident[256];
		insw(ch->io_base + ATA_REG_DATA, ident, 256);

		if (ident[0] == 0x8000) {
			ch->present = true;
			ch->is_atapi = true;

			klog(KLOG_INFO, "ide: ATAPI drive on 0x%x %s\n", ch->io_base, drive ? "slave" : "master");

			return 0;
		}

		if (ident[0] == 0x0000) {
			ch->present = true;
			ch->is_atapi = false;

			klog(KLOG_INFO, "ide: ATA drive on 0x%x %s\n", ch->io_base, drive ? "slave" : "master");

			return 0;
		} 
	}

	return -1;
}