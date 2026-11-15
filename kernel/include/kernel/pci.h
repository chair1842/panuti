/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_PCI_H
#define _KERNEL_PCI_H

#include <stdint.h>

typedef struct pci_device_info {
	uint8_t bus;
	uint8_t device;
	uint8_t function;

	uint16_t vendor_id;
	uint16_t device_id;

	uint8_t revision;
	uint8_t prog_if;
	uint8_t subclass;
	uint8_t class_code;
	uint8_t header_type;

	uint8_t irq_line;
	uint8_t irq_pin;
} pci_device_info_t;

uint32_t pci_cfg_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_cfg_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_cfg_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

void pci_cfg_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
void pci_cfg_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_cfg_write8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);

bool pci_find_class(uint8_t class_code, uint8_t subclass, pci_device_info_t* out);

void pci_init();

#endif
