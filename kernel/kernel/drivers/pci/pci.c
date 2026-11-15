/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/klog.h>
#include <kernel/memman/slab.h>
#include <kernel/pci.h>
#include <panuti/errno.h>
#include <panuti/inode_type.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PCI_MAX_DEVICES 64

typedef enum pci_field {
	PCI_FIELD_VENDOR,
	PCI_FIELD_DEVICE,
	PCI_FIELD_CLASS,
	PCI_FIELD_SUBCLASS,
	PCI_FIELD_PROG_IF,
	PCI_FIELD_REVISION,
	PCI_FIELD_HEADER_TYPE,
	PCI_FIELD_IRQ_LINE,
	PCI_FIELD_IRQ_PIN,
	PCI_FIELD_COUNT,
} pci_field_t;

typedef struct pci_handle {
	char text[8];
	size_t len;
	size_t pos;
} pci_handle_t;

static pci_device_info_t pci_devices[PCI_MAX_DEVICES];
static size_t pci_device_count;
static bool pci_cap_warned;

static const char* const pci_field_names[PCI_FIELD_COUNT] = {
	"vendor",
	"device",
	"class",
	"subclass",
	"prog_if",
	"revision",
	"header_type",
	"irq_line",
	"irq_pin",
};

typedef struct {
	char* buf;
	size_t cap;
	size_t len;
} pci_sink_t;

static void pci_putchar(char c, void* extra) {
	pci_sink_t* s = (pci_sink_t*)extra;

	if (s->len + 1 < s->cap) {
		s->buf[s->len] = c;
		s->len++;
		s->buf[s->len] = '\0';
	}
}

static size_t pci_format(char* buf, size_t cap, const char* fmt, ...) {
	pci_sink_t s = { buf, cap, 0 };

	if (cap == 0) {
		return 0;
	}

	buf[0] = '\0';

	va_list ap;
	va_start(ap, fmt);
	vfctprintf(pci_putchar, &s, fmt, ap);
	va_end(ap);

	return s.len;
}

static size_t pci_render(char* buf, size_t cap, const pci_device_info_t* d, pci_field_t field) {
	switch (field) {
		case PCI_FIELD_VENDOR:
			return pci_format(buf, cap, "0x%04x\n", d->vendor_id);
		case PCI_FIELD_DEVICE:
			return pci_format(buf, cap, "0x%04x\n", d->device_id);
		case PCI_FIELD_CLASS:
			return pci_format(buf, cap, "0x%02x\n", d->class_code);
		case PCI_FIELD_SUBCLASS:
			return pci_format(buf, cap, "0x%02x\n", d->subclass);
		case PCI_FIELD_PROG_IF:
			return pci_format(buf, cap, "0x%02x\n", d->prog_if);
		case PCI_FIELD_REVISION:
			return pci_format(buf, cap, "0x%02x\n", d->revision);
		case PCI_FIELD_HEADER_TYPE:
			return pci_format(buf, cap, "0x%02x\n", d->header_type);
		case PCI_FIELD_IRQ_LINE:
			return pci_format(buf, cap, "0x%02x\n", d->irq_line);
		case PCI_FIELD_IRQ_PIN:
			return pci_format(buf, cap, "0x%02x\n", d->irq_pin);
		case PCI_FIELD_COUNT:
			break;
	}

	return 0;
}

static int pci_open(void* impl, void** out_impl) {
	uintptr_t desc = (uintptr_t)impl;
	size_t index = desc >> 8;
	pci_field_t field = (pci_field_t)(desc & 0xFF);

	if (index >= pci_device_count || field >= PCI_FIELD_COUNT) {
		return PANUTIERRNO_NOTFOUND;
	}

	pci_handle_t* h = kmalloc(sizeof(*h), alignof(pci_handle_t));
	if (!h) {
		return PANUTIERRNO_PLAINERR;
	}

	h->pos = 0;
	h->len = pci_render(h->text, sizeof(h->text), &pci_devices[index], field);

	*out_impl = h;
	return 0;
}

static int pci_read(void* impl, void* buf, size_t len) {
	pci_handle_t* h = (pci_handle_t*)impl;

	if (h->pos >= h->len) {
		return 0;
	}

	size_t avail = h->len - h->pos;
	size_t n = (avail < len) ? avail : len;
	memcpy(buf, h->text + h->pos, n);
	h->pos += n;

	return (int)n;
}

static int pci_close(void* impl, struct task* self) {
	(void)self;
	kfree(impl);
	return 0;
}

static const handle_ops_t pci_file_ops = {
	.open = pci_open,
	.read = pci_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.seek = op_seek_ignore,
	.close = pci_close,
};

static void pci_record(uint8_t bus, uint8_t device, uint8_t function, uint16_t vendor_id) {
	if (pci_device_count >= PCI_MAX_DEVICES) {
		if (!pci_cap_warned) {
			klog(KLOG_WARN, "pci: more than %d devices, ignoring the rest\n", PCI_MAX_DEVICES);
			pci_cap_warned = true;
		}

		return;
	}

	pci_device_info_t* d = &pci_devices[pci_device_count++];

	d->bus = bus;
	d->device = device;
	d->function = function;
	d->vendor_id = vendor_id;
	d->device_id = pci_cfg_read16(bus, device, function, 0x02);
	d->revision = pci_cfg_read8(bus, device, function, 0x08);
	d->prog_if = pci_cfg_read8(bus, device, function, 0x09);
	d->subclass = pci_cfg_read8(bus, device, function, 0x0A);
	d->class_code = pci_cfg_read8(bus, device, function, 0x0B);
	d->header_type = pci_cfg_read8(bus, device, function, 0x0E) & 0x7F;

	if (d->header_type == 0 || d->header_type == 1) {
		d->irq_line = pci_cfg_read8(bus, device, function, 0x3C);
		d->irq_pin = pci_cfg_read8(bus, device, function, 0x3D);
	} else {
		d->irq_line = 0;
		d->irq_pin = 0;
	}
}

static void pci_scan(void) {
	for (unsigned bus = 0; bus < 256; bus++) {
		for (unsigned device = 0; device < 32; device++) {
			uint16_t vendor = pci_cfg_read16((uint8_t)bus, (uint8_t)device, 0, 0x00);

			if (vendor == 0xFFFF) {
				continue;
			}

			uint8_t header = pci_cfg_read8((uint8_t)bus, (uint8_t)device, 0, 0x0E);

			pci_record((uint8_t)bus, (uint8_t)device, 0, vendor);

			if (!(header & 0x80)) {
				continue;
			}

			for (unsigned function = 1; function < 8; function++) {
				uint16_t fn_vendor = pci_cfg_read16((uint8_t)bus, (uint8_t)device, (uint8_t)function, 0x00);

				if (fn_vendor == 0xFFFF) {
					continue;
				}

				pci_record((uint8_t)bus, (uint8_t)device, (uint8_t)function, fn_vendor);
			}
		}
	}
}

static void pci_add_node(size_t index) {
	const pci_device_info_t* d = &pci_devices[index];

	char dir[24];
	pci_format(dir, sizeof(dir), "/dvc/pci/%02x:%02x.%x", d->bus, d->device, d->function);

	if (registry_mkdir(dir) != 0) {
		klog(KLOG_WARN, "pci: cannot create %s\n", dir);
		return;
	}

	for (int field = 0; field < PCI_FIELD_COUNT; field++) {
		char path[48];
		pci_format(path, sizeof(path), "%s/%s", dir, pci_field_names[field]);

		void* desc = (void*)(uintptr_t)((index << 8) | (size_t)field);
		if (registry_add(path, INODE_DVC, desc, &pci_file_ops) != 0) {
			klog(KLOG_WARN, "pci: cannot create %s\n", path);
		}
	}
}

bool pci_find_class(uint8_t class_code, uint8_t subclass, pci_device_info_t* out) {
	for (size_t i = 0; i < pci_device_count; i++) {
		const pci_device_info_t* d = &pci_devices[i];

		if (d->class_code != class_code || d->subclass != subclass) {
			continue;
		}

		if (out) {
			*out = *d;
		}

		return true;
	}

	return false;
}

void pci_init(void) {
	pci_device_count = 0;
	pci_cap_warned = false;

	registry_mkdir("/dvc/pci");

	pci_scan();

	for (size_t i = 0; i < pci_device_count; i++) {
		pci_add_node(i);
	}

	klog(KLOG_INFO, "pci: %u device(s)\n", (unsigned)pci_device_count);
}
