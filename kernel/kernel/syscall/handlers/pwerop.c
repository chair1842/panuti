/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/syscall/handlers.h>
#include <panuti/syscall/pwerop.h>
#include <panuti/errno.h>
#include <kernel/acpi.h>

int32_t syshandler_pwerop(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3; (void)a4;
	uint32_t op = a1;
	uint32_t magic = a2;

	if (magic != PANUTI_PWEROP_MAGIC) {
		return PANUTIERRNO_INVALIDARG;
	}

	if (!acpi_available()) {
		return PANUTIERRNO_NOTSUPPORTED;
	}

	// the returns are here in case shit fails
	if (op == PANUTI_PWEROP_REBOOT) {
		acpi_reboot();
		return PANUTIERRNO_PLAINERR;
	} else if (op == PANUTI_PWEROP_SHUTDOWN) {
		acpi_poweroff();
		return PANUTIERRNO_PLAINERR;
	}

	return PANUTIERRNO_INVALIDARG;
}