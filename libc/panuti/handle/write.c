/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/handle.h>
#include <panuti/syscall/syscallsf.h>

int handle_write(int fd, const void* data, size_t len) {
	return panutisysf_write(fd, data, len);
}