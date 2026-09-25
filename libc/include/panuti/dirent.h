#ifndef _PANUTI_DIRENT_H
#define _PANUTI_DIRENT_H

#include <panuti/inode_type.h>

#define DIRENT_NAME_MAX 256

typedef struct {
	char name[DIRENT_NAME_MAX];
	inode_type_t type;
} dirent_entry_t;

#endif