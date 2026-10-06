/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_MMAP_H
#define _PANUTI_MMAP_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Protection bits accepted by mmapan(). The syscall argument is a plain
 * uint32_t, so these have to stay inside a 32-bit word. */
#define MMAPAN_PROT_NONE  0x0
#define MMAPAN_PROT_READ  0x1
#define MMAPAN_PROT_WRITE 0x2
#define MMAPAN_PROT_EXEC  0x4

#define MMAPAN_MAX (16 * 1024 * 1024)

int32_t mmapan(size_t len, int prot, void* addr_hint);

int32_t munmap(void* addr, size_t len);

#if defined(__cplusplus)
}
#endif

#endif
