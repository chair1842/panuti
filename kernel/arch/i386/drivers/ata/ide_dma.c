/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ide.h"
#include <stdint.h>
#include "../../io.h"
#include <kernel/klog.h>
#include <kernel/memman/memman.h>
#include <kernel/memman/vmalloc.h>
#include <kernel/mem/usr.h>
#include <kernel/pci.h>
#include <kernel/timer.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <kernel/irq.h>

#define IDE_BM_BOUNCE_PAGES 32

#define PCI_CLASS_STORAGE 0x01
#define PCI_SUBCLASS_IDE 0x01
#define PCI_PROGIF_BUS_MASTER 0x80

#define PCI_REG_COMMAND 0x04
#define PCI_REG_BAR4 0x20

#define PCI_CMD_IO_SPACE 0x0001
#define PCI_CMD_BUS_MASTER 0x0004

static int ide_dma_channel_init(ide_channel_t* ch, uint16_t bm_base, int bus) {
	ch->bm_base = (uint16_t)(bm_base + (bus ? 8 : 0));
	ch->dma_ok = false;

	ch->prd = vmalloc_pages(1);
	if (!ch->prd) {
		klog(KLOG_WARN, "ide: bus %d: cannot allocate PRD page, DMA disabled\n", bus);
		ch->bm_base = 0;
		return -1;
	}

	ch->prd_phys = memman_get_phys((uint32_t)ch->prd);
	if (!ch->prd_phys) {
		klog(KLOG_WARN, "ide: bus %d: PRD page is not physically mapped, DMA disabled\n", bus);
		vmalloc_free_pages(ch->prd, 1);
		ch->prd = NULL;
		ch->bm_base = 0;
		return -1;
	}

	// user buffers are bounced through this. if it cannot be allocated DMA
	// still works for kernel buffers, and the transfer paths keep user
	// buffers on PIO
	ch->bounce = vmalloc_pages(IDE_BM_BOUNCE_PAGES);
	ch->bounce_size = 0;

	if (ch->bounce) {
		ch->bounce_size = IDE_BM_BOUNCE_PAGES * 4096;
	} else {
		klog(KLOG_WARN, "ide: bus %d: no bounce buffer, user buffers will use PIO\n", bus);
	}

	ch->dma_ok = true;
	klog(KLOG_INFO, "ide: bus %d: bus master at 0x%x, PRD 0x%x, bounce %u bytes\n",
		 bus, ch->bm_base, ch->prd_phys, ch->bounce_size);

	return 0;
}

static int build_prd(ide_channel_t* ch, void* buf, size_t bytes) {
	size_t left = bytes;
	uintptr_t virt = (uintptr_t)buf;
	size_t entries = 0;

	if (bytes == 0) {
		return -1;
	}

	while (left > 0) {
		if (entries >= IDE_PRD_ENTRIES) {
			return -1;
		}

		uint32_t page = (uint32_t)(virt & ~(uintptr_t)0xFFF);
		uint32_t offset = (uint32_t)(virt & 0xFFF);
		size_t len = 4096 - offset;

		if (len > left) {
			len = left;
		}

		uint32_t phys = memman_get_phys(page);
		if (!phys) {
			return -1;
		}

		ch->prd[entries].addr = phys + offset;
		ch->prd[entries].count = (uint16_t)len;
		ch->prd[entries].flags = 0;
		entries++;

		virt += len;
		left -= len;
	}

	ch->prd[entries - 1].flags = IDE_PRD_EOT;
	return (int)entries;
}

bool ide_dma_needs_bounce(const void* buf, size_t bytes) {
	uintptr_t start = (uintptr_t)buf;

	if (start < USER_SPACE_END) {
		return true;
	}

	return bytes > 0 && start > (uintptr_t)-1 - (bytes - 1);
}

int ide_dma_prepare(ide_channel_t* ch, void* buf, size_t bytes) {
	if (!ch->dma_ok || !ch->prd) {
		return -1;
	}

	if (build_prd(ch, buf, bytes) < 0) {
		return -1;
	}

	outl(ch->bm_base + IDE_BM_PRDT, ch->prd_phys);

	uint8_t status = inb(ch->bm_base + IDE_BM_STATUS);
	outb(
		ch->bm_base + IDE_BM_STATUS, 
		(uint8_t)((status & IDE_BM_STATUS_TIMING) | IDE_BM_STATUS_ERROR | IDE_BM_STATUS_IRQ)
	);

	return 0;
}

void ide_dma_start(ide_channel_t* ch, bool write) {
	if (!ch->dma_ok) {
		return;
	}

	uint8_t command = IDE_BM_CMD_START;
	if (!write) {
		command |= IDE_BM_CMD_READ;
	}

	outb(ch->bm_base + IDE_BM_CMD, command);
}

int ide_dma_wait(ide_channel_t* ch, uint32_t ticks) {
	if (!ch->dma_ok) {
		ch->dma_waiter = NULL;
		return -1;
	}

	task_t* task = sched_current();
	if (!task) {
		ch->dma_waiter = NULL;
		return IDE_DMA_KILLED;
	}
	
	uint32_t deadline = timer_get_ticks() + ticks;
	uint32_t flags;
	int result;

	while (true) {
		flags = irq_save_disable();

		if (ch->dma_done) {
			result = 0;
			break;
		}

		if (task->kill_pending) {
			result = IDE_DMA_KILLED;
			break;
		}

		if ((int32_t)(timer_get_ticks() - deadline) >= 0) {
			result = IDE_DMA_TIMEOUT;
			break;
		}

		task->wake_at = deadline;
		task_block(task);

		irq_restore(flags);
	}

	ch->dma_waiter = NULL;
	task->wake_at = 0;
	irq_restore(flags);

	return result;
}

void ide_dma_stop(ide_channel_t* ch) {
	if (!ch->dma_ok) {
		return;
	}

	outb(ch->bm_base + IDE_BM_CMD, 0);

	uint8_t status = inb(ch->bm_base + IDE_BM_STATUS);
	outb(
		ch->bm_base + IDE_BM_STATUS,
		(uint8_t)((status & IDE_BM_STATUS_TIMING) | IDE_BM_STATUS_ERROR | IDE_BM_STATUS_IRQ)
	);
}

int ide_dma_init(void) {
	pci_device_info_t dev;

	if (!pci_find_class(PCI_CLASS_STORAGE, PCI_SUBCLASS_IDE, &dev)) {
		klog(KLOG_WARN, "ide: no class-0101 controller found, bus master DMA unavailable\n");
		return -1;
	}

	if (dev.header_type != 0) {
		klog(
			KLOG_WARN,
			"ide: %02x:%02x.%x is not a type-0 device, bus master DMA unavailable\n",
			dev.bus, dev.device, dev.function
		);
		
		return -1;
	}

	if (!(dev.prog_if & PCI_PROGIF_BUS_MASTER)) {
		klog(
			KLOG_WARN,
			"ide: %02x:%02x.%x is not bus master capable (prog-if 0x%02x), DMA unavailable\n",
			dev.bus, dev.device, dev.function, dev.prog_if
		);
		
		return -1;
	}

	uint32_t bar4 = pci_cfg_read32(dev.bus, dev.device, dev.function, PCI_REG_BAR4);
	if (!(bar4 & 1) || (bar4 & ~0x3u) == 0) {
		klog(KLOG_WARN, "ide: BAR4 0x%08x is not an assigned I/O BAR, DMA unavailable\n", bar4);
		return -1;
	}

	uint16_t bm_base = (uint16_t)(bar4 & ~0x3u);

	uint16_t command = pci_cfg_read16(dev.bus, dev.device, dev.function, PCI_REG_COMMAND);
	uint16_t enabled = command | PCI_CMD_IO_SPACE | PCI_CMD_BUS_MASTER;

	if (enabled != command) {
		pci_cfg_write16(dev.bus, dev.device, dev.function, PCI_REG_COMMAND, enabled);
	}

	klog(
		KLOG_INFO,
		"ide: bus master IDE at %02x:%02x.%x, BAR4 0x%x, command 0x%x -> 0x%x\n",
		dev.bus, dev.device, dev.function, bm_base, command,
		pci_cfg_read16(dev.bus, dev.device, dev.function, PCI_REG_COMMAND)
	);

	int channels = 0;

	for (int bus = 0; bus < IDE_CHANNEL_COUNT; bus++) {
		ide_channel_t* ch = ide_channel_get(bus);
		if (!ch) {
			continue;
		}

		if (!ch->drives[0].present && !ch->drives[1].present) {
			continue;
		}

		if (ide_dma_channel_init(ch, bm_base, bus) == 0) {
			channels++;
		}
	}

	if (channels == 0) {
		klog(KLOG_WARN, "ide: no DMA-capable channel, falling back to PIO\n");
		return -1;
	}

	return 0;
}

void ide_dma_arm(ide_channel_t* ch) {
	uint8_t status = inb(ch->bm_base + IDE_BM_STATUS);
	outb(
		ch->bm_base + IDE_BM_STATUS,
		(uint8_t)((status & IDE_BM_STATUS_TIMING) | IDE_BM_STATUS_ERROR | IDE_BM_STATUS_IRQ)
	);

	uint32_t flags = irq_save_disable();
	ch->dma_done = false;
	ch->dma_status = 0;
	ch->dma_ata_status = 0;
	ch->dma_waiter = sched_current();
	irq_restore(flags);
}