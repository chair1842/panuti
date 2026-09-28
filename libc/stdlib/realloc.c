/* SPDX-License-Identifier: BSD-3-Clause */

#if !defined (__is_libk)

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "malloc_private.h"

void* realloc(void* ptr, size_t size) {
	struct block* b;
	size_t old_payload;
	size_t copy;
	void* n;

	if (!ptr) {
		return malloc(size);
	}
	
	if (size == 0) {
		free(ptr);
		return NULL;
	}

	// the block header sits just in front of the payload
	b = (struct block*)((uint8_t*)ptr - BLOCK_HDR);
	old_payload = (size_t)BLOCK_SIZE(b) - BLOCK_HDR;

	n = malloc(size);
	if (!n) {
		// the original block is still valid and still owned by the caller
		return NULL;
	}
	
	copy = old_payload < size ? old_payload : size;
	memcpy(n, ptr, copy);
	free(ptr);
	return n;
}

#endif
