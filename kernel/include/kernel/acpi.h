/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_ACPI_H
#define _KERNEL_ACPI_H

#include <stdbool.h>

void acpi_init(void);
bool acpi_available(void);
void acpi_poweroff(void);
void acpi_reboot(void);

#endif
