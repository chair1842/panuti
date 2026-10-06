/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_FS_H
#define _PANUTI_FS_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

int mkdir(const char* path);

int mkfile(const char* path);

#if defined(__cplusplus)
}
#endif

#endif