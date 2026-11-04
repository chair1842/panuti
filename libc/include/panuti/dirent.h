/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_DIRENT_H
#define _PANUTI_DIRENT_H

#include <panuti/inode_type.h>
#include <stddef.h>
#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define DIRENT_NAME_MAX 256

typedef struct {
	char name[DIRENT_NAME_MAX];
	inode_type_t type;
	
	size_t size;

	time_t atime;
	time_t mtime;
	time_t ctime;
	time_t btime;
} dirent_entry_t;

#if defined(__cplusplus)
}
#endif

#endif
