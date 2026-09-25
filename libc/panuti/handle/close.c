/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/handle.h>
#include <panuti/syscall/syscallsf.h>

int handle_close(int fd) {
	return panutisysf_close(fd);
}