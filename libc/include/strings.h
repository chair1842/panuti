/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _STRINGS_H
#define _STRINGS_H 1

#include <sys/cdefs.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

int strcasecmp(const char*, const char*);
int strncasecmp(const char*, const char*, size_t);

#if defined(__cplusplus)
}
#endif

#endif
