/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_MMAP_H
#define _PANUTI_MMAP_H

#include <stddef.h>
#include <stdint.h>

/* Protection bits accepted by mmapan(). The syscall argument is a plain
 * uint32_t, so these have to stay inside a 32-bit word. */
#define MMAPAN_PROT_NONE  0x0
#define MMAPAN_PROT_READ  0x1
#define MMAPAN_PROT_WRITE 0x2
#define MMAPAN_PROT_EXEC  0x4

/* Ceiling on a single call. The kernel keeps no per-task accounting, so this
 * bounds one request and not a process's total footprint. */
#define MMAPAN_MAX (16 * 1024 * 1024)

/*
 * Map `len` bytes of anonymous memory and return the base address, or a
 * negative PANUTIERRNO_* code.
 *
 * The whole region is backed by physical frames and zeroed before the call
 * returns -- there is no demand paging, so every page is resident. `addr_hint`
 * is a preference, not a contract: it is rounded down to a page, and ignored
 * if it does not currently hold a free run that large, in which case the
 * kernel searches the rest of user space instead.
 *
 * PROT_EXEC is accepted but has no effect, because a 32-bit page table entry
 * carries no NX bit and text is executable either way. PROT_NONE yields pages
 * that are present but supervisor-only, so the caller cannot touch them at
 * all until they are unmapped.
 */
int32_t mmapan(size_t len, int prot, void* addr_hint);

/*
 * Release a range previously handed out by mmapan(). `addr` must be page
 * aligned and `len` is rounded up to a whole number of pages. Pages in the
 * range that are already unmapped are skipped, so a range may span a hole.
 * Returns 0, or a negative PANUTIERRNO_* code.
 */
int32_t munmap(void* addr, size_t len);

#endif
