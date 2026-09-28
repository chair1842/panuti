/* SPDX-License-Identifier: BSD-3-Clause */

#if !defined (__is_libk)

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "malloc_private.h"

void* malloc(size_t size) {
	size_t need;
	uint32_t i;

	// Reject sizes that would wrap once the header and alignment padding are
	// added, so a huge request cannot masquerade as a small one.
	if (size > (size_t)-1 - BLOCK_HDR - (HEAP_ALIGN - 1)) {
		return NULL;
	}
	
	need = heap_align_up(size + BLOCK_HDR);
	if (need < HEAP_MIN_BLOCK) {
		need = HEAP_MIN_BLOCK;
	}

	// First fit over the existing free lists. A block big enough to split
	// leaves the tail on the list; otherwise it is taken whole.
	for (i = 0; i < heap_region_count; i++) {
		struct region* r = &heap_regions[i];
		struct block* b = r->flist;
		
		while (b) {
			uint32_t total = BLOCK_SIZE(b);
			if (total >= need) {
				if (total - need >= HEAP_MIN_BLOCK) {
					struct block* rem = (struct block*)((uint8_t*)b + need);
					struct block* after;
					
					rem->size = total - need;
					rem->prev_size = (uint32_t)need;
					
					heap_flist_set_next(rem, heap_flist_next(b));
					after = (struct block*)((uint8_t*)rem + (total - need));
					
					// the block after the remainder now
					// follows a differently sized block
					if ((uint8_t*)after < (uint8_t*)r->base + r->brk) {
						after->prev_size = total - need;
					}
					
					heap_flist_replace(r, b, rem);
					b->size = (uint32_t)need | BLOCK_INUSE;
				} else {
					heap_flist_remove(r, b);
					b->size = total | BLOCK_INUSE;
				}
				
				return BLOCK_PAYLOAD(b);
			}
			
			b = heap_flist_next(b);
		}
	}

	// Nothing on the free lists fits, so carve fresh memory.
	for (i = 0; i < heap_region_count; i++) {
		struct region* r = &heap_regions[i];
		if ((size_t)r->brk + need <= (size_t)r->size) {
			void* p = heap_region_bump(r, need);
			if (p) {
				return p;
			}
		}
	}

	// No region has room; ask the kernel for more.
	{
		struct region* r = heap_grow(need);
		if (!r) {
			return NULL;
		}
		
		return heap_region_bump(r, need);
	}
}

#endif
