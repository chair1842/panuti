#ifndef _PANUTI_MOUNT_H
#define _PANUTI_MOUNT_H

#include <stdint.h>

// mounts an fs at the mount path backed by the block device
int mount(const char* mount_path, const char* fstype, const char* blkdev_path);

#endif