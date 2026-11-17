/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "../../io.h"
#include <kernel/acpi.h>
#include <kernel/klog.h>
#include <kernel/memman/tempmap.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <uacpi/kernel_api.h>
#include <uacpi/sleep.h>
#include <uacpi/status.h>
#include <uacpi/uacpi.h>

#define RSDP_BASE_LEN 20
#define RSDP_EXT_LEN 36
#define RSDP_MAX_LEN 4096

#define EBDA_POINTER_PHYS 0x0000040E
#define EBDA_SCAN_LEN 1024
#define BIOS_SCAN_START 0x000E0000u
#define BIOS_SCAN_END 0x00100000u

#define SCAN_CHUNK 0x2000u

static uint32_t rsdp_phys;
static bool acpi_ready;

static bool rsdp_signature(const uint8_t* p) {
	return p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == ' ' && p[4] == 'P' && p[5] == 'T' && p[6] == 'R' && p[7] == ' ';
}

static bool checksum_ok(const uint8_t* p, uint32_t len) {
	uint8_t sum = 0;

	for (uint32_t i = 0; i < len; i++) {
		sum = (uint8_t)(sum + p[i]);
	}

	return sum == 0;
}

static bool rsdp_check(const uint8_t* p, uint32_t available) {
	if (available < RSDP_BASE_LEN || !rsdp_signature(p) || !checksum_ok(p, RSDP_BASE_LEN)) {
		return false;
	}

	if (p[15] < 2) {
		return true;
	}

	if (available < RSDP_EXT_LEN) {
		return false;
	}

	uint32_t length = (uint32_t)p[20] | ((uint32_t)p[21] << 8) | ((uint32_t)p[22] << 16) | ((uint32_t)p[23] << 24);

	if (length < RSDP_EXT_LEN || length > RSDP_MAX_LEN || length > available) {
		return false;
	}

	return checksum_ok(p, length);
}

static uint32_t scan_range(uint32_t start, uint32_t length) {
	for (uint32_t off = 0; off < length; off += SCAN_CHUNK) {
		uint32_t chunk = length - off;
		if (chunk > SCAN_CHUNK) {
			chunk = SCAN_CHUNK;
		}

		uint32_t map_len = chunk + RSDP_EXT_LEN;
		uint8_t* p = map_physical_temp(start + off, map_len);

		for (uint32_t i = 0; i + RSDP_BASE_LEN <= map_len; i += 16) {
			if (rsdp_signature(p + i) && rsdp_check(p + i, map_len - i)) {
				unmap_physical_temp(p, map_len);
				return start + off + i;
			}
		}

		unmap_physical_temp(p, map_len);
	}

	return 0;
}

static uint32_t find_rsdp(void) {
	uint16_t* ebda_ptr = map_physical_temp(EBDA_POINTER_PHYS, sizeof(*ebda_ptr));
	uint16_t ebda_segment = *ebda_ptr;
	unmap_physical_temp(ebda_ptr, sizeof(*ebda_ptr));

	if (ebda_segment != 0) {
		uint32_t ebda_phys = (uint32_t)ebda_segment << 4;
		uint32_t found = scan_range(ebda_phys, EBDA_SCAN_LEN);
		if (found != 0) {
			return found;
		}
	}

	return scan_range(BIOS_SCAN_START, BIOS_SCAN_END - BIOS_SCAN_START);
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out_rsdp_address) {
	if (!out_rsdp_address || rsdp_phys == 0) {
		return UACPI_STATUS_NOT_FOUND;
	}

	*out_rsdp_address = (uacpi_phys_addr)rsdp_phys;
	return UACPI_STATUS_OK;
}

void acpi_init(void) {
	if (rsdp_phys != 0) {
		return;
	}

	rsdp_phys = find_rsdp();
	if (rsdp_phys == 0) {
		klog(KLOG_WARN, "acpi: RSDP not found, continuing without ACPI\n");
		return;
	}

	klog(KLOG_INFO, "acpi: RSDP at 0x%x\n", rsdp_phys);

	uacpi_status status = uacpi_initialize(0);
	if (status != UACPI_STATUS_OK) {
		klog(KLOG_WARN, "acpi: uacpi_initialize failed: %s\n", uacpi_status_to_string(status));
		return;
	}

	acpi_ready = true;

	status = uacpi_namespace_load();
	if (status != UACPI_STATUS_OK) {
		klog(KLOG_WARN, "acpi: namespace load failed: %s\n", uacpi_status_to_string(status));
		return;
	}

	status = uacpi_namespace_initialize();
	if (status != UACPI_STATUS_OK) {
		klog(KLOG_WARN, "acpi: namespace init failed: %s\n", uacpi_status_to_string(status));
		return;
	}

	klog(KLOG_INFO, "acpi: initialized\n");
}

bool acpi_available(void) {
	return acpi_ready;
}

void acpi_poweroff(void) {
	if (!acpi_ready) {
		return;
	}

	uacpi_status status = uacpi_enter_sleep_state_simple(UACPI_SLEEP_STATE_S5);
	klog(KLOG_WARN, "acpi: poweroff failed: %s\n", uacpi_status_to_string(status));
}

void acpi_reboot(void) {
	if (acpi_ready) {
		uacpi_status status = uacpi_reboot();
		klog(KLOG_WARN, "acpi: reboot failed: %s\n", uacpi_status_to_string(status));
	}

	klog(KLOG_INFO, "acpi: why are you calling this rn? acpi is not ready")
}
