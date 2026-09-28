/* SPDX-License-Identifier: BSD-3-Clause */

// Internal definitions shared by the heap allocator's translation units.
// This header is private to libc/stdlib and is not installed.

#ifndef _PANUTI_MALLOC_PRIVATE_H
#define _PANUTI_MALLOC_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

// Small allocations are served from 1 MiB regions obtained from mmapan(). A
// request that cannot fit in a fresh region gets a dedicated region sized to
// hold it, so large allocations work without blocks ever spanning regions.
#define HEAP_ALIGN       8u
#define HEAP_MIN_BLOCK   16u
#define HEAP_REGION_SIZE (1024u * 1024u)
#define HEAP_MAX_REGIONS 128u
#define HEAP_MAX_TOTAL   (128u * 1024u * 1024u)
#define HEAP_PAGE_SIZE   0x1000u

// Low bit of a block's size field. The remaining bits hold the total block
// size, header included.
#define BLOCK_INUSE 1u

// Header stored immediately in front of every block's payload. `size` covers
// the header as well and carries the in-use flag. `prev_size` is the total
// size of the physically preceding block in the same region, or zero when this
// is the region's first block; it is what lets free() walk backwards.
typedef struct block {
	uint32_t size;
	uint32_t prev_size;
} block_t;

// One contiguous run handed out by the kernel. Blocks tile [base, base + size)
// exactly, while `brk` is the offset of the first byte not yet carved.
// `flist` is the region's free list, ordered by address and singly linked
// through the `next` pointer stored in each free block's payload.
typedef struct region {
	uint32_t base;
	uint32_t size;
	uint32_t brk;
	struct block* flist;
} region_t;

#define BLOCK_SIZE(b) ((b)->size & ~(uint32_t)BLOCK_INUSE)
#define BLOCK_USED(b) ((b)->size & (uint32_t)BLOCK_INUSE)
#define BLOCK_HDR ((uint32_t)sizeof(struct block))
#define BLOCK_PAYLOAD(b) ((void*)((uint8_t*)(b) + BLOCK_HDR))

// Supplied by the linker script, immediately after the program's BSS. The
// first heap region is requested there so it lands as close to the image as
// possible.
extern char _heap_start[];

// Every region ever obtained from the kernel, plus how many are in use.
// Regions are never handed back, so this table only ever grows.
extern struct region heap_regions[HEAP_MAX_REGIONS];
extern uint32_t heap_region_count;

size_t heap_align_up(size_t n);

struct block* heap_flist_next(struct block* b);
void heap_flist_set_next(struct block* b, struct block* n);
void heap_flist_remove(struct region* r, struct block* b);
void heap_flist_replace(struct region* r, struct block* old, struct block* nw);
void heap_flist_insert(struct region* r, struct block* b);

struct region* heap_region_of(const void* p);
void* heap_region_bump(struct region* r, size_t need);
struct region* heap_grow(size_t min_size);

#endif
