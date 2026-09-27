/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_STAT_H
#define _PANUTI_STAT_H

#include <panuti/dirent.h>

// describe a path as a dirent, 0 on success
int stat(const char* path, dirent_entry_t* dirent_out);

// report whether a path resolves to something
bool nexist(const char* path);

// read the next entry of an open directory, 0 on entry, 1 once it is done
int readdir(int fd, dirent_entry_t* dirent_out);

#endif
