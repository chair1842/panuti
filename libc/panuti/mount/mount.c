/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/mount.h>
#include <panuti/syscall/syscallsf.h>

int32_t mount(const char* mount_path, const char* fstype, const char* blkdev_path) {
	return panutisysf_mount(mount_path, fstype, blkdev_path);
}