#include "ide.h"
#include <stdbool.h>
#include <stdint.h>
#include "../../io.h"
#include <kernel/timer.h>
#include <kernel/klog.h>

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

		// right after a reset the drive blurts out its type in the
		// sector count and lba registers:
		//   sec=1, lba_lo=1, lba_mid=0x14, lba_hi=0xEB for atapi
		//   sec=1, lba_lo=0, lba_mid=0x00, lba_hi=0x00 for plain old ata
		for (volatile int i = 0; i < 4; i++) {
			iowait();
		}

		uint8_t sec = ide_read_reg(ch, ATA_REG_SECCOUNT);
		uint8_t lo  = ide_read_reg(ch, ATA_REG_LBA_LO);
		uint8_t mid = ide_read_reg(ch, ATA_REG_LBA_MID);
		uint8_t hi  = ide_read_reg(ch, ATA_REG_LBA_HI);

		if (sec == 0) {
			continue; // nothing answering on this slot
		}

		if (lo == 0x01 && mid == 0x14 && hi == 0xEB) {
			ch->present = true;
			ch->is_atapi = true;

			klog(KLOG_INFO, "ide: ATAPI drive on 0x%x %s\n", ch->io_base, drive ? "slave" : "master");

			return 0;
		}

		if (lo == 0x00 && mid == 0x00 && hi == 0x00) {
			ch->present = true;
			ch->is_atapi = false;

			klog(KLOG_INFO, "ide: ATA drive on 0x%x %s\n", ch->io_base, drive ? "slave" : "master");

			return 0;
		}
	}

	return -1;
}