/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/registry.h>
#include <kernel/fs/isofs.h>
#include <kernel/fs/fatfs.h>
#include <kernel/fs/ext2.h>
#include <panuti/errno.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>
#include <string.h>

// mount the filesystem identified by `fstype` from the block device at
// `blkdev` (relative to the caller's cwd) onto the directory `mountp`.
int32_t syshandler_mount(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a4;
	const char* mountp = (const char*)a1;
	const char* fstype = (const char*)a2;
	const char* blkdev = (const char*)a3;

	size_t mountp_len = kernel_user_strlen(mountp);
	size_t fstype_len = kernel_user_strlen(fstype);
	size_t blkdev_len = kernel_user_strlen(blkdev);

	if (mountp_len == (size_t)-1 || fstype_len == (size_t)-1 || blkdev_len == (size_t)-1 ||
	    !kernel_usrptr_verify(mountp, mountp_len + 1) ||
	    !kernel_usrptr_verify(fstype, fstype_len + 1) ||
	    !kernel_usrptr_verify(blkdev, blkdev_len + 1)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (strcmp(fstype, "isofs") == 0) {
		return isofs_mount(mountp, blkdev);
	}

	if (strcmp(fstype, "fatfs") == 0) {
		return fatfs_mount(mountp, blkdev);
	}

	if (strcmp(fstype, "ext2") == 0) {
		return ext2_mount(mountp, blkdev);
	}

	return PANUTIERRNO_NOTSUPPORTED;
}
