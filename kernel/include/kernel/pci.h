/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_PCI_H
#define _KERNEL_PCI_H

#include <stdint.h>

uint32_t pci_cfg_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_cfg_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_cfg_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

#endif
