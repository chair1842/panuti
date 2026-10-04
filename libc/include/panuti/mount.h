/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_MOUNT_H
#define _PANUTI_MOUNT_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

// mounts an fs at the mount path backed by the block device.
int32_t mount(const char* mount_path, const char* fstype, const char* blkdev_path);

// unmounts whatever filesystem is attached at the mount path.
int32_t unmount(const char* mount_path);

#if defined(__cplusplus)
}
#endif

#endif
