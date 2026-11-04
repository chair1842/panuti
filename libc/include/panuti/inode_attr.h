/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_INODE_ATTR_H
#define _PANUTI_INODE_ATTR_H

#include <stdint.h>
#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define INODE_ATTR_SIZE  (1u << 0)
#define INODE_ATTR_ATIME (1u << 1)
#define INODE_ATTR_MTIME (1u << 2)
#define INODE_ATTR_CTIME (1u << 3)
#define INODE_ATTR_BTIME (1u << 4)
#define INODE_ATTR_ALL   (INODE_ATTR_SIZE | INODE_ATTR_ATIME | INODE_ATTR_MTIME | INODE_ATTR_CTIME | INODE_ATTR_BTIME)
#define INODE_ATTR_TIMES (INODE_ATTR_ATIME | INODE_ATTR_MTIME | INODE_ATTR_CTIME | INODE_ATTR_BTIME)

typedef struct inode_timestamp {
	time_t atime;
	time_t mtime;
	time_t ctime;
	time_t btime;
} inode_timestamp_t;


// set the timestamps selected by mask on the entry at path
int settmsp(const char* path, inode_timestamp_t* timestamp, unsigned int mask);

#if defined(__cplusplus)
}
#endif

#endif
