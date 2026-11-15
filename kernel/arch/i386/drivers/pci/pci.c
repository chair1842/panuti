/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "../../io.h"
#include <kernel/pci.h>
#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_CONFIG_ENABLE 0x80000000u

static uint32_t pci_config_address( uint8_t bus, uint8_t device, uint8_t function, uint8_t offset ) {
	return PCI_CONFIG_ENABLE |
		((uint32_t)bus << 16) |
		((uint32_t)device << 11) |
		((uint32_t)function << 8) |
		(uint32_t)(offset & 0xFC);
}

uint32_t pci_cfg_read32( uint8_t bus, uint8_t device, uint8_t function, uint8_t offset ) {
	outl(PCI_CONFIG_ADDRESS, pci_config_address(bus, device, function, offset));
	return inl(PCI_CONFIG_DATA);
}

uint16_t pci_cfg_read16( uint8_t bus, uint8_t device, uint8_t function, uint8_t offset ) {
	uint32_t dword = pci_cfg_read32(bus, device, function, offset);
	return (uint16_t)(dword >> ((offset & 2) * 8));
}

uint8_t pci_cfg_read8( uint8_t bus, uint8_t device, uint8_t function, uint8_t offset ) {
	uint32_t dword = pci_cfg_read32(bus, device, function, offset);
	return (uint8_t)(dword >> ((offset & 3) * 8));
}
