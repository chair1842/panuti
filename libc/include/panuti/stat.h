/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_STAT_H
#define _PANUTI_STAT_H

#include <panuti/dirent.h>

// describe a path as a dirent, 0 on success
int stat(const char* path, dirent_entry_t* dirent_out);

#endif
