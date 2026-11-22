/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ide.h"
#include <stdint.h>
#include "../../io.h"
#include "../../intpt/handlers/main.h"
#include <kernel/timer.h>
#include <kernel/klog.h>
#include <kernel/irq.h>
#include <kernel/sched/sched.h>

#define ATA_IRQ14_VECTOR 46
#define ATA_IRQ15_VECTOR 47

static ide_channel_t primary = {
	.io_base = ATA_PRIMARY_IO,
	.ctrl_base = ATA_PRIMARY_CTRL,
	.control = ATA_DCR_NIEN, // NIEN 💔🫩
	.locked = false,
	.lock_waiters = { nullptr },
};

static ide_channel_t secondary = {
	.io_base = ATA_SECONDARY_IO,
	.ctrl_base = ATA_SECONDARY_CTRL,
	.control = ATA_DCR_NIEN,
	.locked = false,
	.lock_waiters = { nullptr },
};

static void ide_dma_irq(ide_channel_t* ch) {
	if (!ch->dma_ok) {
		ide_read_reg(ch, ATA_REG_STATUS);
		return;
	}

	uint8_t bm = inb(ch->bm_base + IDE_BM_STATUS);

	if (bm & IDE_BM_STATUS_IRQ) {
		uint8_t clear = IDE_BM_STATUS_IRQ;
		if (bm & IDE_BM_STATUS_ERROR) {
			clear |= IDE_BM_STATUS_ERROR;
		}

		outb(ch->bm_base + IDE_BM_STATUS, clear);

		uint8_t ata = ide_read_reg(ch, ATA_REG_STATUS);

		ch->dma_status = bm;
		ch->dma_ata_status = ata;
		ch->dma_done = true;

		if (ch->dma_waiter) {
			ch->dma_waiter->wake_at = 0;
			task_wake(ch->dma_waiter);
		}
	} else {
		ide_read_reg(ch, ATA_REG_STATUS);
	}
}

static void ata_irq14_handler(registers_t* regs) {
	(void)regs;
	ide_dma_irq(&primary);
	primary.irq_fired = true;
	ide_read_reg(&primary, ATA_REG_STATUS);
}

static void ata_irq15_handler(registers_t* regs) {
	(void)regs;
	ide_dma_irq(&secondary);
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
	task_t* self = sched_current();
	uint32_t flags = irq_save_disable();

	if (!ch->locked) {
		ch->locked = true;
		irq_restore(flags);
		return;
	}

	if (!self) {
		while (ch->locked) {
			irq_restore(flags);
			__asm__ __volatile__("hlt");
			flags = irq_save_disable();
		}

		ch->locked = true;
		irq_restore(flags);
		return;
	}

	wait_entry_t entry;
	wait_entry_init(&entry, self);
	wait_queue_add(&ch->lock_waiters, &entry);

	while (ch->locked) {
		task_block(self);

		irq_restore(flags);
		flags = irq_save_disable();
	}

	wait_queue_remove(&ch->lock_waiters, &entry);
	ch->locked = true;
	irq_restore(flags);
}

void ide_channel_release(ide_channel_t* ch) {
	uint32_t flags = irq_save_disable();

	ch->locked = false;
	wait_queue_wake_one(&ch->lock_waiters);

	irq_restore(flags);
}

void ide_soft_reset(ide_channel_t* ch) {
	ide_dma_stop(ch);

	ide_control_write(ch, ch->control | ATA_DCR_SRST | ATA_DCR_NIEN);

	for (volatile int i = 0; i < 1000000; i++) {
		iowait();
	}

	ide_control_write(ch, (ch->control & ~(ATA_DCR_SRST | ATA_DCR_HOB)) | ATA_DCR_NIEN);
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
		drv->dma_ok = false;

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

static int ide_identify_cmd(ide_drive_t* drv, uint16_t* words, uint8_t command, const char* what) {
	ide_channel_t* ch = drv->channel;
	ide_channel_acquire(ch);

	int rc = -1;

	if (ide_poll(ch) < 0) {
		klog(
			KLOG_WARN,
			"ide: drive stuck busy before %s on 0x%x %s\n",
			 what,
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
	ide_write_reg(ch, ATA_REG_COMMAND, command);

	int status = ide_wait_drq(ch, IDE_TIMEOUT_TICKS);
	if (status < 0) {
		klog(
			KLOG_WARN,
			"ide: %s never opened its data phase on 0x%x %s\n",
			 what,
			 ch->io_base,
			drv->index ? "slave" : "master"
		);
		
		goto out;
	}

	if (status & ATA_SR_ERR) {
		klog(
			KLOG_WARN,
			"ide: %s rejected on 0x%x %s: %s\n",
			what,
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
			"ide: %s never finished on 0x%x %s\n",
			what,
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

int ide_identify(ide_drive_t* drv, uint16_t* words) {
	if (!drv || !words || drv->is_atapi) {
		return -1;
	}

	return ide_identify_cmd(drv, words, ATA_CMD_IDENTIFY, "IDENTIFY");
}

int ide_identify_packet(ide_drive_t* drv, uint16_t* words) {
	if (!drv || !words || !drv->is_atapi) {
		return -1;
	}

	return ide_identify_cmd(drv, words, ATA_CMD_IDENTIFY_PACKET, "IDENTIFY PACKET");
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

	if (found > 0) {
		ide_dma_init();
	}

	return found;
}

void ide_control_write(ide_channel_t* ch, uint8_t value) {
	ch->control = value;
	outb(ch->ctrl_base + ATA_REG_DEV_CTRL, value);
}

void ide_control_update(ide_channel_t* ch, uint8_t set, uint8_t clear) {
	ide_control_write(ch, (uint8_t)((ch->control | set) & ~clear));
}