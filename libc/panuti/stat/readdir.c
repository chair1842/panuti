/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/stat.h>
#include <panuti/syscall/syscallsf.h>

int readdir(int fd, dirent_entry_t* dirent_out) {
	return panutisysf_readdir(fd, dirent_out);
}
