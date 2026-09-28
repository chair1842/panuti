/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/mmap.h>
#include <panuti/syscall/syscallsf.h>

int32_t munmap(void* addr, size_t len) {
	return panutisysf_munmap(addr, len);
}
