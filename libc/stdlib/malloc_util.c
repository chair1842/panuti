/* SPDX-License-Identifier: BSD-3-Clause */

// Heap bookkeeping shared by malloc(), free() and realloc(): the region table,
// the per-region free lists, the bump carver, and the code that asks the kernel
// for more memory.

#if !defined (__is_libk)

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <panuti/mmap.h>

#include "malloc_private.h"

struct region heap_regions[HEAP_MAX_REGIONS];
uint32_t heap_region_count;

size_t heap_align_up(size_t n) {
	return (n + (HEAP_ALIGN - 1)) & ~(size_t)(HEAP_ALIGN - 1);
}

// The free list threads itself through free blocks' payloads, so the link is
// read and written with memcpy to stay clear of aliasing assumptions.
struct block* heap_flist_next(struct block* b) {
	struct block* n;
	memcpy(&n, BLOCK_PAYLOAD(b), sizeof(n));
	return n;
}

void heap_flist_set_next(struct block* b, struct block* n) {
	memcpy(BLOCK_PAYLOAD(b), &n, sizeof(n));
}

void heap_flist_remove(struct region* r, struct block* b) {
	struct block* prev = NULL;
	struct block* cur = r->flist;

	while (cur && cur != b) {
		prev = cur;
		cur = heap_flist_next(cur);
	}
	
	if (cur != b) {
		return;
	}
	
	if (prev) {
		heap_flist_set_next(prev, heap_flist_next(cur));
	} else {
		r->flist = heap_flist_next(cur);
	}
	
	heap_flist_set_next(cur, NULL);
}

// Put `nw` where `old` currently sits in the list, keeping address order.
void heap_flist_replace(struct region* r, struct block* old, struct block* nw) {
	struct block* prev = NULL;
	struct block* cur = r->flist;

	while (cur && cur != old) {
		prev = cur;
		cur = heap_flist_next(cur);
	}
	
	if (cur != old) {
		return;
	}
	
	if (prev) {
		heap_flist_set_next(prev, nw);
	} else {
		r->flist = nw;
	}
	
	heap_flist_set_next(old, NULL);
}

void heap_flist_insert(struct region* r, struct block* b) {
	struct block* prev = NULL;
	struct block* cur = r->flist;

	while (cur && (uint8_t*)cur < (uint8_t*)b) {
		prev = cur;
		cur = heap_flist_next(cur);
	}
	
	heap_flist_set_next(b, cur);
	if (prev) {
		heap_flist_set_next(prev, b);
	} else {
		r->flist = b;
	}
}

struct region* heap_region_of(const void* p) {
	uint32_t i;

	for (i = 0; i < heap_region_count; i++) {
		struct region* r = &heap_regions[i];
		if ((uint8_t*)p >= (uint8_t*)r->base &&
		    (uint8_t*)p < (uint8_t*)(r->base + r->size)) {
			return r;
		}
	}
	
	return NULL;
}

// Carve `need` bytes off the end of a region. The new block records the size of
// the block it follows, which is found by walking the region's blocks up to the
// bump pointer.
void* heap_region_bump(struct region* r, size_t need) {
	uint8_t* base = (uint8_t*)r->base;
	struct block* b;
	struct block* p;
	uint32_t prev = 0;

	if ((size_t)r->brk + need > (size_t)r->size) {
		return NULL;
	}

	if (r->brk != 0) {
		p = (struct block*)base;
		while ((uint8_t*)p + BLOCK_SIZE(p) < base + r->brk) {
			p = (struct block*)((uint8_t*)p + BLOCK_SIZE(p));
		}
		
		prev = BLOCK_SIZE(p);
	}

	b = (struct block*)(base + r->brk);
	b->size = (uint32_t)need | BLOCK_INUSE;
	b->prev_size = prev;
	r->brk += (uint32_t)need;
	return BLOCK_PAYLOAD(b);
}

// Ask the kernel for another region. `min_size` lets a single oversized
// request get a region big enough to hold it. The hint asks for the run right
// after the last region to keep the heap contiguous; if that is unavailable the
// kernel picks a free run anywhere in user space.
struct region* heap_grow(size_t min_size) {
	struct region* r;
	int32_t ret;
	void* hint = (void*)_heap_start;
	size_t want = min_size > HEAP_REGION_SIZE ? min_size : HEAP_REGION_SIZE;
	uint32_t i;
	uint32_t total = 0;

	if (heap_region_count >= HEAP_MAX_REGIONS) {
		return NULL;
	}

	// mmapan works in whole pages, and refuses anything over its own cap.
	want = (want + (HEAP_PAGE_SIZE - 1)) & ~(size_t)(HEAP_PAGE_SIZE - 1);
	if (want > (size_t)MMAPAN_MAX) {
		return NULL;
	}

	for (i = 0; i < heap_region_count; i++) {
		total += heap_regions[i].size;
	}
	if ((size_t)total + want > (size_t)HEAP_MAX_TOTAL) {
		return NULL;
	}

	if (heap_region_count > 0) {
		struct region* last = &heap_regions[heap_region_count - 1];
		hint = (void*)(uintptr_t)(last->base + last->size);
	}

	ret = mmapan(want, MMAPAN_PROT_READ | MMAPAN_PROT_WRITE, hint);
	if (ret < 0 && heap_region_count > 0) {
		ret = mmapan(want, MMAPAN_PROT_READ | MMAPAN_PROT_WRITE, NULL);
	}
	if (ret < 0) {
		return NULL;
	}

	r = &heap_regions[heap_region_count++];
	r->base = (uint32_t)(uintptr_t)ret;
	r->size = (uint32_t)want;
	r->brk = 0;
	r->flist = NULL;
	return r;
}

#endif
