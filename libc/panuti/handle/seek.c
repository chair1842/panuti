/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/handle.h>
#include <panuti/syscall/syscallsf.h>

int handle_seek(int fd, off_t offset, int whence) {
	return panutisysf_seek(fd, &offset, whence);
}