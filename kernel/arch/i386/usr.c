/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/mem/usr.h>
#include <kernel/memman/memman.h>
#include <stdint.h>

bool kernel_is_user_ptr(const void* ptr) {
	uint32_t addr = (uint32_t)ptr;
	return addr >= USER_SPACE_BASE && addr < USER_SPACE_END;
}

bool kernel_is_user_range(const void* buf, size_t len) {
	if (len == 0) {
		return true;
	}
	uint32_t start = (uint32_t)buf;
	uint32_t end = start + (uint32_t)len;
	return start >= USER_SPACE_BASE && end >= start && end <= USER_SPACE_END;
}

static bool pages_mapped(uint32_t start, uint32_t end) {
	for (uint32_t a = start & ~0xFFFu; a < end; a += 0x1000u) {
		if (memman_get_phys(a) == 0) {
			return false;
		}
	}

	return true;
}

bool kernel_usrptr_verify(const void* ptr, size_t len) {
	// proves the address is a userspace address and that the range neither
	// wraps nor runs past USER_SPACE_END. NULL is rejected for any len > 0
	// here, since zero fails the base comparison.
	if (!kernel_is_user_range(ptr, len)) {
		return false;
	}

	// a zero length range carries no data, so there is nothing to fault on and
	// the address itself is never dereferenced
	if (len == 0) {
		return true;
	}

	// the range check above only says where the caller pointed. this is what
	// says a page table entry actually backs it, so a gap cannot take the
	// kernel down through a page fault in kernel mode on the caller's behalf
	uint32_t start = (uint32_t)ptr;
	return pages_mapped(start, start + (uint32_t)len);
}

size_t kernel_user_strlen(const void* ptr) {
	uint32_t addr = (uint32_t)ptr;
	if (addr < USER_SPACE_BASE || addr >= USER_SPACE_END) {
		return (size_t)-1;
	}

	uint32_t remaining = USER_SPACE_END - addr;
	const uint8_t* p = (const uint8_t*)ptr;
	for (uint32_t i = 0; i < remaining; i++) {
		// a string is followed to its terminator, so an unmapped page partway
		// down would fault in kernel mode. confirm each new page before the
		// dereference and report the string as unusable instead
		if (i == 0 || ((addr + i) & 0xFFFu) == 0) {
			if (memman_get_phys(addr + i) == 0) {
				return (size_t)-1;
			}
		}

		if (p[i] == '\0') {
			return (size_t)i;
		}
	}

	return (size_t)-1;
}