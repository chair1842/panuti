/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/memman/memman.h>
#include <kernel/mem/usr.h>
#include <panuti/errno.h>
#include <stddef.h>
#include <stdint.h>

int32_t syshandler_munmap(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3;
	(void)a4;
	uint32_t addr = a1;
	size_t len = a2;

	if (len == 0) {
		return PANUTIERRNO_INVALIDADDR;
	}

	/* Callers only ever get a base address back from mmapan(), so a range
	 * starting mid-page means the arithmetic went wrong on their side rather
	 * than that they meant to clip a neighbour. */
	if (addr & 0xFFF) {
		return PANUTIERRNO_INVALIDADDR;
	}

	/* Also rejects a length that would wrap the 32-bit address space. */
	if (!kernel_is_user_range((void*)addr, len)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	/* Bounded by the range check above, so rounding up stays inside user space
	 * and cannot overflow. */
	uint32_t npages = ((uint32_t)len + 0xFFF) >> 12;

	/* A gap in the range is not an error: it may legitimately span a hole. */
	memman_unmap_run_free(addr, npages);

	return PANUTIERRNO_PLAINSUCCESS;
}
