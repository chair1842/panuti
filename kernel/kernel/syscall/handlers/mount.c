#include <kernel/handle/registry.h>
#include <kernel/fs/isofs.h>
#include <kernel/fs/fatfs.h>
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

	if (!kernel_is_user_ptr(mountp) || !kernel_is_user_ptr(fstype) || !kernel_is_user_ptr(blkdev) ||
	    kernel_user_strlen(mountp) == (size_t)-1 ||
	    kernel_user_strlen(fstype) == (size_t)-1 ||
	    kernel_user_strlen(blkdev) == (size_t)-1) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (strcmp(fstype, "iso9660") == 0) {
		return isofs_mount(mountp, blkdev);
	}

	if (strcmp(fstype, "vfat") == 0) {
		return fatfs_mount(mountp, blkdev);
	}

	return PANUTIERRNO_NOTSUPPORTED;
}
