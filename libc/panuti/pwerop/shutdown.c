/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/pwerop.h>
#include <panuti/syscall/pwerop.h>
#include <panuti/syscall/syscallsf.h>

void pwerop_reboot() {
	panutisysf_pwerop(PANUTI_PWEROP_SHUTDOWN, PANUTI_PWEROP_MAGIC);
}