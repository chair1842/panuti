/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_FS_H
#define _PANUTI_FS_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

// creates a directory at `path`, resolving it against the caller's cwd unless
// it is absolute. the parent has to exist already; only the final component is
// created.
//
// returns PANUTIERRNO_PLAINSUCCESS (0) on success. on failure it returns
// either a PANUTIERRNO_* code or a raw -1, which is what the registry layer
// hands back for every path-level rejection (no such parent, the parent is not
// a directory, the name already exists, the path is too long, the inode or
// dirent tables are full). -1 is not a PANUTIERRNO_* code, so callers testing
// for success should compare against 0 rather than against NOTFOUND.
int32_t mkdir(const char* path);

#if defined(__cplusplus)
}
#endif

#endif