/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/mount.h>
#include <panuti/syscall/syscallsf.h>

int32_t unmount(const char* mount_path) {
	return panutisysf_unmount(mount_path);
}
