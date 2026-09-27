/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/stat.h>
#include <panuti/syscall/syscallsf.h>

int stat(const char* path, dirent_entry_t* dirent_out) {
	return panutisysf_stat(path, dirent_out);
}
