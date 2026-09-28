/* SPDX-License-Identifier: BSD-3-Clause */

#if !defined (__is_libk)

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "malloc_private.h"

// Return `ptr` to its region's free list, merging it with the free blocks on
// either side so a run of released memory becomes reusable as one block.
void free(void* ptr) {
	struct block* b;
	struct region* r;
	uint8_t* base;
	uint8_t* limit;
	uint32_t total;

	if (!ptr) {
		return;
	}

	b = (struct block*)((uint8_t*)ptr - BLOCK_HDR);
	r = heap_region_of(b);
	if (!r) {
		// not one of ours; nothing sensible to do
		return;
	}

	base = (uint8_t*)r->base;
	limit = base + r->brk;
	total = BLOCK_SIZE(b);
	b->size = total; // clear the in-use bit

	// merge with the previous physical block if it is free
	if (b->prev_size != 0 && (uint8_t*)b - (size_t)b->prev_size >= base) {
		struct block* prev = (struct block*)((uint8_t*)b - (size_t)b->prev_size);
		if (!BLOCK_USED(prev)) {
			heap_flist_remove(r, prev);
			prev->size = BLOCK_SIZE(prev) + total;
			b = prev;
			total = prev->size;
		}
	}

	// merge with the next physical block if it is free
	{
		struct block* next = (struct block*)((uint8_t*)b + total);
		if ((uint8_t*)next < limit && !BLOCK_USED(next)) {
			heap_flist_remove(r, next);
			total += BLOCK_SIZE(next);
			b->size = total;
		}
	}

	// the block after the merged one has a new predecessor size
	{
		struct block* after = (struct block*)((uint8_t*)b + total);
		if ((uint8_t*)after < limit) {
			after->prev_size = total;
		}
	}

	heap_flist_insert(r, b);
}

#endif
