/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/mount.h>
#include <kernel/syscall/handlers.h>

int32_t syshandler_sync(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a1; (void)a2; (void)a3; (void)a4;

	return mount_sync_all();
}
