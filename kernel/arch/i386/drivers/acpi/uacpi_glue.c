/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "../../intpt/handlers/main.h"
#include "../../io.h"
#include <kernel/irq.h>
#include <kernel/klog.h>
#include <kernel/ksleep.h>
#include <kernel/memman/layout.h>
#include <kernel/memman/memman.h>
#include <kernel/memman/slab.h>
#include <kernel/pci.h>
#include <kernel/sched/sched.h>
#include <kernel/timer.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <uacpi/kernel_api.h>
#include <uacpi/status.h>
#include <uacpi/types.h>

#define ACPI_MAP_PAGES 4096u

static uint32_t acpi_map_bits[ACPI_MAP_PAGES / 32u];

static bool acpi_bit_test(uint32_t bit) {
	return (acpi_map_bits[bit / 32u] & (1u << (bit % 32u))) != 0;
}

static void acpi_bit_set(uint32_t bit) {
	acpi_map_bits[bit / 32u] |= 1u << (bit % 32u);
}

static void acpi_bit_clear(uint32_t bit) {
	acpi_map_bits[bit / 32u] &= ~(1u << (bit % 32u));
}

static uint32_t acpi_map_find(uint32_t npages) {
	uint32_t run = 0;

	for (uint32_t i = 0; i < ACPI_MAP_PAGES; i++) {
		if (acpi_bit_test(i)) {
			run = 0;
			continue;
		}

		if (++run == npages) {
			return i + 1 - npages;
		}
	}

	return UINT32_MAX;
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
	if (len == 0) {
		return UACPI_MAP_FAILED;
	}

	uint32_t phys = (uint32_t)addr;
	uint32_t page = phys & ~(PAGE_SIZE - 1u);
	uint32_t offset = phys - page;
	uint64_t total = (uint64_t)offset + len;
	uint32_t npages = (uint32_t)((total + PAGE_SIZE - 1u) / PAGE_SIZE);

	if (npages == 0 || npages > ACPI_MAP_PAGES) {
		return UACPI_MAP_FAILED;
	}

	uint32_t flags = irq_save_disable();
	uint32_t index = acpi_map_find(npages);
	if (index == UINT32_MAX) {
		irq_restore(flags);
		klog(KLOG_WARN, "acpi: virtual mapping space exhausted\n");
		return UACPI_MAP_FAILED;
	}

	uint32_t virt = ACPI_MAP_BASE + index * PAGE_SIZE;
	for (uint32_t i = 0; i < npages; i++) {
		acpi_bit_set(index + i);
		memman_map(virt + i * PAGE_SIZE, page + i * PAGE_SIZE, MEMMAN_PRESENT | MEMMAN_RW);
	}

	for (uint32_t i = 0; i < npages; i++) {
		if (memman_get_phys(virt + i * PAGE_SIZE) != page + i * PAGE_SIZE) {
			for (uint32_t j = 0; j <= i; j++) {
				if (j < i) {
					memman_unmap(virt + j * PAGE_SIZE);
				}
				
				acpi_bit_clear(index + j);
			}
			
			irq_restore(flags);
			return UACPI_MAP_FAILED;
		}
	}

	irq_restore(flags);
	return (void*)(virt + offset);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len) {
	if (!addr || addr == UACPI_MAP_FAILED || len == 0) {
		return;
	}

	uint32_t virt = (uint32_t)addr;
	uint32_t page = virt & ~(PAGE_SIZE - 1u);
	uint32_t offset = virt - page;
	uint32_t npages = (uint32_t)(((uint64_t)offset + len + PAGE_SIZE - 1u) / PAGE_SIZE);

	if (page < ACPI_MAP_BASE) {
		klog(KLOG_WARN, "acpi: unmap of non-ACPI address %p\n", addr);
		return;
	}

	uint32_t index = (page - ACPI_MAP_BASE) / PAGE_SIZE;
	if (index >= ACPI_MAP_PAGES || npages > ACPI_MAP_PAGES - index) {
		klog(KLOG_WARN, "acpi: unmap range out of bounds %p\n", addr);
		return;
	}

	uint32_t flags = irq_save_disable();
	for (uint32_t i = 0; i < npages; i++) {
		memman_unmap(page + i * PAGE_SIZE);
		acpi_bit_clear(index + i);
	}
	
	irq_restore(flags);
}

void* uacpi_kernel_alloc(uacpi_size size) {
	if (size == 0) {
		size = 1;
	}

	return kmalloc((uint32_t)size, sizeof(uint64_t));
}

void uacpi_kernel_free(void* mem) {
	if (mem) {
		kfree(mem);
	}
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
	return (uacpi_u64)timer_get_ticks() * 10000000ull;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
	while (usec--) {
		inb(0x80);
	}
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
	if (msec == 0) {
		return;
	}

	if (sched_current()) {
		while (msec > 0) {
			uint32_t chunk_ms = msec > 1000 ? 1000 : (uint32_t)msec;
			ksleep((chunk_ms + 9) / 10);
			msec -= chunk_ms;
		}
		
		return;
	}

	uint64_t deadline = timer_get_ticks() + (msec + 9) / 10;
	while (timer_get_ticks() < deadline) {
		__asm__ __volatile__("hlt");
	}
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
	task_t* current = sched_current();

	// uacpi forbids UACPI_THREAD_ID_NONE, so boot code gets a fake id
	return current ? (uacpi_thread_id)current : (uacpi_thread_id)(uintptr_t)1;
}

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
	return (uacpi_interrupt_state)irq_save_disable();
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state) {
	irq_restore((uint32_t)state);
}

typedef struct acpi_semaphore {
	volatile uacpi_u32 units;
} acpi_semaphore_t;

uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 initial_units) {
	acpi_semaphore_t* sem = kmalloc(sizeof(*sem), sizeof(uint64_t));
	if (!sem) {
		return UACPI_NULL;
	}

	sem->units = initial_units;
	return sem;
}

void uacpi_kernel_free_semaphore(uacpi_handle handle) {
	if (handle) {
		kfree(handle);
	}
}

uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle handle, uacpi_u16 timeout) {
	acpi_semaphore_t* sem = (acpi_semaphore_t*)handle;
	if (!sem) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	bool infinite = timeout == 0xFFFF;
	uint64_t deadline = 0;
	if (timeout != 0 && !infinite) {
		deadline = timer_get_ticks() + ((uint64_t)timeout + 9) / 10;
	}

	while (true) {
		uint32_t flags = irq_save_disable();
		if (sem->units > 0) {
			sem->units--;
			irq_restore(flags);
			return UACPI_STATUS_OK;
		}
		
		irq_restore(flags);

		if (timeout == 0) {
			return UACPI_STATUS_TIMEOUT;
		}

		if (!infinite && timer_get_ticks() >= deadline) {
			return UACPI_STATUS_TIMEOUT;
		}

		if (sched_current()) {
			ksleep(1);
		} else {
			uacpi_kernel_stall(100);
		}
	}
}

void uacpi_kernel_signal_semaphore(uacpi_handle handle) {
	acpi_semaphore_t* sem = (acpi_semaphore_t*)handle;
	if (!sem) {
		return;
	}

	uint32_t flags = irq_save_disable();
	if (sem->units != UINT32_MAX) {
		sem->units++;
	}
	
	irq_restore(flags);
}

typedef struct acpi_spinlock {
	volatile uacpi_u32 held;
} acpi_spinlock_t;

uacpi_handle uacpi_kernel_create_spinlock(void) {
	acpi_spinlock_t* lock = kmalloc(sizeof(*lock), sizeof(uint64_t));
	if (!lock) {
		return UACPI_NULL;
	}

	lock->held = 0;
	return lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
	if (handle) {
		kfree(handle);
	}
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
	acpi_spinlock_t* lock = (acpi_spinlock_t*)handle;
	uacpi_cpu_flags flags = (uacpi_cpu_flags)irq_save_disable();

	if (!lock) {
		return flags;
	}

	while (lock->held) {
		__asm__ __volatile__("pause");
	}
	
	lock->held = 1;

	return flags;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
	acpi_spinlock_t* lock = (acpi_spinlock_t*)handle;

	if (lock) {
		lock->held = 0;
	}

	irq_restore((uint32_t)flags);
}

#define ACPI_IRQ_MAX 16

typedef struct acpi_irq_slot {
	uacpi_interrupt_handler handler;
	uacpi_handle ctx;
	bool installed;
} acpi_irq_slot_t;

static acpi_irq_slot_t acpi_irq_slots[ACPI_IRQ_MAX];

static void acpi_irq_dispatch(registers_t* regs) {
	uint32_t irq = regs->vector - 32;

	if (irq < ACPI_IRQ_MAX && acpi_irq_slots[irq].installed && acpi_irq_slots[irq].handler) {
		acpi_irq_slots[irq].handler(acpi_irq_slots[irq].ctx);
	}
}

uacpi_status uacpi_kernel_install_interrupt_handler(
	uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx,
	uacpi_handle* out_irq_handle) {
	if (!handler || !out_irq_handle || irq >= ACPI_IRQ_MAX) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	uint32_t flags = irq_save_disable();
	if (acpi_irq_slots[irq].installed) {
		irq_restore(flags);
		return UACPI_STATUS_ALREADY_EXISTS;
	}

	acpi_irq_slots[irq].handler = handler;
	acpi_irq_slots[irq].ctx = ctx;
	acpi_irq_slots[irq].installed = true;
	register_handler((uint8_t)(32 + irq), acpi_irq_dispatch);
	irq_restore(flags);

	*out_irq_handle = (uacpi_handle)(uintptr_t)(irq + 1);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
	uacpi_interrupt_handler handler, uacpi_handle irq_handle) {
	uintptr_t value = (uintptr_t)irq_handle;
	if (value == 0 || value > ACPI_IRQ_MAX) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	uint32_t irq = (uint32_t)(value - 1);
	uint32_t flags = irq_save_disable();
	if (!acpi_irq_slots[irq].installed || acpi_irq_slots[irq].handler != handler) {
		irq_restore(flags);
		return UACPI_STATUS_NOT_FOUND;
	}

	acpi_irq_slots[irq].installed = false;
	acpi_irq_slots[irq].handler = UACPI_NULL;
	acpi_irq_slots[irq].ctx = UACPI_NULL;
	register_handler((uint8_t)(32 + irq), NULL);
	irq_restore(flags);

	return UACPI_STATUS_OK;
}

typedef struct acpi_pci_device {
	uint8_t bus;
	uint8_t device;
	uint8_t function;
} acpi_pci_device_t;

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle* out_handle) {
	if (!out_handle) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	if (address.segment != 0) {
		return UACPI_STATUS_NOT_FOUND;
	}

	acpi_pci_device_t* dev = kmalloc(sizeof(*dev), sizeof(uint64_t));
	if (!dev) {
		return UACPI_STATUS_OUT_OF_MEMORY;
	}

	dev->bus = address.bus;
	dev->device = address.device;
	dev->function = address.function;
	*out_handle = dev;
	return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle) {
	if (handle) {
		kfree(handle);
	}
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8* value) {
	acpi_pci_device_t* dev = (acpi_pci_device_t*)handle;
	if (!dev || !value) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	*value = pci_cfg_read8(dev->bus, dev->device, dev->function, (uint8_t)offset);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16* value) {
	acpi_pci_device_t* dev = (acpi_pci_device_t*)handle;
	if (!dev || !value) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	*value = pci_cfg_read16(dev->bus, dev->device, dev->function, (uint8_t)offset);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32* value) {
	acpi_pci_device_t* dev = (acpi_pci_device_t*)handle;
	if (!dev || !value) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	*value = pci_cfg_read32(dev->bus, dev->device, dev->function, (uint8_t)offset);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 value) {
	acpi_pci_device_t* dev = (acpi_pci_device_t*)handle;
	if (!dev) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	pci_cfg_write8(dev->bus, dev->device, dev->function, (uint8_t)offset, value);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 value) {
	acpi_pci_device_t* dev = (acpi_pci_device_t*)handle;
	if (!dev) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	pci_cfg_write16(dev->bus, dev->device, dev->function, (uint8_t)offset, value);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 value) {
	acpi_pci_device_t* dev = (acpi_pci_device_t*)handle;
	if (!dev) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	pci_cfg_write32(dev->bus, dev->device, dev->function, (uint8_t)offset, value);
	return UACPI_STATUS_OK;
}

typedef struct acpi_io_region {
	uacpi_u32 base;
	uacpi_u32 length;
} acpi_io_region_t;

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle* out_handle) {
	if (!out_handle || base > 0xFFFF || len == 0 || base + len > 0x10000ull) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	acpi_io_region_t* region = kmalloc(sizeof(*region), sizeof(uint64_t));
	if (!region) {
		return UACPI_STATUS_OUT_OF_MEMORY;
	}

	region->base = (uacpi_u16)base;
	region->length = (uacpi_u16)len;
	*out_handle = region;
	return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
	if (handle) {
		kfree(handle);
	}
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8* out_value) {
	acpi_io_region_t* region = (acpi_io_region_t*)handle;
	if (!region || !out_value || offset >= region->length) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	*out_value = inb(region->base + (uint16_t)offset);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16* out_value) {
	acpi_io_region_t* region = (acpi_io_region_t*)handle;
	if (!region || !out_value || offset + 2 > region->length) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	*out_value = inw(region->base + (uint16_t)offset);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32* out_value) {
	acpi_io_region_t* region = (acpi_io_region_t*)handle;
	if (!region || !out_value || offset + 4 > region->length) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	*out_value = inl(region->base + (uint16_t)offset);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value) {
	acpi_io_region_t* region = (acpi_io_region_t*)handle;
	if (!region || offset >= region->length) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	outb(region->base + (uint16_t)offset, in_value);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value) {
	acpi_io_region_t* region = (acpi_io_region_t*)handle;
	if (!region || offset + 2 > region->length) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	outw(region->base + (uint16_t)offset, in_value);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value) {
	acpi_io_region_t* region = (acpi_io_region_t*)handle;
	if (!region || offset + 4 > region->length) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	outl(region->base + (uint16_t)offset, in_value);
	return UACPI_STATUS_OK;
}

void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* message) {
	klog(level <= UACPI_LOG_WARN ? KLOG_WARN : KLOG_INFO, "uacpi: %s", message);
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request* request) {
	if (!request) {
		return UACPI_STATUS_INVALID_ARGUMENT;
	}

	if (request->type == UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT) {
		klog(KLOG_WARN, "acpi: firmware breakpoint\n");
	} else if (request->type == UACPI_FIRMWARE_REQUEST_TYPE_FATAL) {
		klog(
			KLOG_WARN,
			"acpi: firmware fatal type=%u code=0x%x arg=0x%llx\n",
			request->fatal.type,
			request->fatal.code,
			(unsigned long long)request->fatal.arg
		);
	} else {
		klog(KLOG_WARN, "acpi: unknown firmware request %u\n", request->type);
	}

	return UACPI_STATUS_OK;
}

static uacpi_u8 acpi_work_item_token;

uacpi_handle uacpi_kernel_create_work_item(void) {
	return &acpi_work_item_token;
}

void uacpi_kernel_free_work_item(uacpi_handle work_item) {
	(void)work_item;
}

void uacpi_kernel_schedule_work(
	uacpi_work_type type, uacpi_handle work_item,
	uacpi_work_handler handler, uacpi_handle ctx) {
	(void)type;
	(void)work_item;

	handler(ctx);
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
	return UACPI_STATUS_OK;
}
