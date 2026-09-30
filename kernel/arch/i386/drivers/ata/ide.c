/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ide.h"
#include <stdint.h>
#include "../../io.h"
#include "../../intpt/handlers/main.h"
#include <kernel/timer.h>
#include <kernel/klog.h>

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

int ide_wait_ready(ide_channel_t* ch, uint32_t ticks) {
	uint32_t timeout = timer_get_ticks() + ticks;
	uint8_t status;

	do {
		__asm__ __volatile__("hlt");
		status = ide_read_reg(ch, ATA_REG_STATUS);
		if (timer_get_ticks() > timeout) {
			return -1;
		}
	} while (status & ATA_SR_BSY);

	return status;
}

int ide_wait_drq(ide_channel_t* ch, uint32_t ticks) {
	uint32_t timeout = timer_get_ticks() + ticks;
	uint8_t status;

	do {
		__asm__ __volatile__("hlt");
		status = ide_read_reg(ch, ATA_REG_STATUS);
		if (timer_get_ticks() > timeout) {
			return -1;
		}
	} while ((status & ATA_SR_BSY) || !(status & ATA_SR_DRQ));

	return status;
}

void ide_channel_acquire(ide_channel_t* ch) {
	while (__sync_lock_test_and_set(&ch->busy, (uint32_t)1)) {
		__asm__ __volatile__("hlt");
	}
}

void ide_channel_release(ide_channel_t* ch) {
	__sync_lock_release(&ch->busy);
}

void ide_soft_reset(ide_channel_t* ch) {
	outb(ch->ctrl_base + ATA_REG_DEV_CTRL, ATA_DCR_SRST | ATA_DCR_NIEN);

	for (volatile int i = 0; i < 1000000; i++) {
		iowait();
	}

	outb(ch->ctrl_base + ATA_REG_DEV_CTRL, ATA_DCR_NIEN);
}

const char* ide_error_string(uint8_t error) {
	if (error & ATA_ER_AMNF) {
		return "address mark not found";
	}
	
	if (error & ATA_ER_TK0NF) {
		return "track 0 not found";
	}
	
	if (error & ATA_ER_ABRT) {
		return "command aborted";
	}
	
	if (error & ATA_ER_MCR) {
		return "media change requested";
	}
	
	if (error & ATA_ER_IDNF) {
		return "id not found";
	}
	
	if (error & ATA_ER_MC) {
		return "media changed";
	}
	
	if (error & ATA_ER_UNC) {
		return "unrecoverable error";
	}
	
	if (error & ATA_ER_BBK) {
		return "bad block";
	}
	
	return "unknown error";
}

ide_channel_t* ide_channel_get(int bus) {
	if (bus == 0) {
		return &primary;
	} else if (bus == 1) {
		return &secondary;
	}
	
	return NULL;
}

int ide_probe_channel(ide_channel_t* ch) {
	ide_soft_reset(ch);

	int found = 0;

	for (int slot = 0; slot < IDE_SLAVES_PER_CHANNEL; slot++) {
		ide_drive_t* drv = &ch->drives[slot];

		drv->channel = ch;
		drv->index = (uint8_t)slot;
		drv->present = false;
		drv->is_atapi = false;

		ide_write_reg(ch, ATA_REG_DRIVE_HEAD, slot ? ATA_HEAD_SLAVE : ATA_HEAD_MASTER);

		for (volatile int i = 0; i < 4; i++) {
			iowait();
		}

		uint8_t sec = ide_read_reg(ch, ATA_REG_SECCOUNT);
		uint8_t mid = ide_read_reg(ch, ATA_REG_LBA_MID);
		uint8_t hi  = ide_read_reg(ch, ATA_REG_LBA_HI);

		if (sec == 0) {
			continue; // nothing answering on this slot
		}

		if (mid == 0x14 && hi == 0xEB) {
			drv->present = true;
			drv->is_atapi = true;
		} else if (mid == 0x00 && hi == 0x00) {
			drv->present = true;
			drv->is_atapi = false;
		} else {
			continue; // not an ata device we know how to talk to
		}

		found++;

		klog(
			KLOG_INFO,
			"ide: %s drive on 0x%x %s\n",
			drv->is_atapi ? "ATAPI" : "ATA",
			ch->io_base, slot ? "slave" : "master"
		);
	}

	return found;
}

int ide_identify(ide_drive_t* drv, uint16_t* words) {
	if (!drv || !words || drv->is_atapi) {
		return -1; // atapi devices speak identify packet (0xa1), not this
	}

	ide_channel_t* ch = drv->channel;
	ide_channel_acquire(ch);

	int rc = -1;

	if (ide_poll(ch) < 0) {
		klog(
			KLOG_WARN,
			"ide: drive stuck busy before IDENTIFY on 0x%x %s\n",
			 ch->io_base,
			drv->index ? "slave" : "master"
		);
		
		goto out;
	}

	ide_write_reg(ch, ATA_REG_FEATURES, 0x00);
	ide_write_reg(ch, ATA_REG_SECCOUNT, 0x00);
	ide_write_reg(ch, ATA_REG_LBA_LO, 0x00);
	ide_write_reg(ch, ATA_REG_LBA_MID, 0x00);
	ide_write_reg(ch, ATA_REG_LBA_HI, 0x00);
	ide_write_reg(ch, ATA_REG_DRIVE_HEAD, (drv->index ? ATA_HEAD_SLAVE : ATA_HEAD_MASTER) | ATA_HEAD_LBA);
	ide_write_reg(ch, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

	int status = ide_wait_drq(ch, IDE_TIMEOUT_TICKS);
	if (status < 0) {
		klog(
			KLOG_WARN,
			"ide: IDENTIFY never opened its data phase on 0x%x %s\n",
			 ch->io_base,
			drv->index ? "slave" : "master"
		);
		
		goto out;
	}

	if (status & ATA_SR_ERR) {
		klog(
			KLOG_WARN,
			"ide: IDENTIFY rejected on 0x%x %s: %s\n",
			ch->io_base,
			drv->index ? "slave" : "master",
			ide_error_string(ide_read_reg(ch, ATA_REG_ERROR))
		);
		
		goto out;
	}

	insw(ch->io_base + ATA_REG_DATA, words, 256);

	if (ide_wait_ready(ch, IDE_TIMEOUT_TICKS) < 0) {
		klog(
			KLOG_WARN,
			"ide: IDENTIFY never finished on 0x%x %s\n",
			ch->io_base,
			drv->index ? "slave" : "master"
		);
		
		goto out;
	}

	rc = 0;

out:
	ide_channel_release(ch);
	return rc;
}

int ide_init(void) {
	register_handler(ATA_IRQ14_VECTOR, ata_irq14_handler);
	register_handler(ATA_IRQ15_VECTOR, ata_irq15_handler);

	int found = 0;

	for (int bus = 0; bus < IDE_CHANNEL_COUNT; bus++) {
		ide_channel_t* ch = ide_channel_get(bus);
		if (ch) {
			found += ide_probe_channel(ch);
		}
	}

	return found;
}
