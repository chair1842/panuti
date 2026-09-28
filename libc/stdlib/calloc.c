/* SPDX-License-Identifier: BSD-3-Clause */

#if !defined (__is_libk)

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void* calloc(size_t nmemb, size_t size) {
	size_t total;
	void* p;

	// guard the multiplication before it wraps
	if (nmemb != 0 && size > (size_t)-1 / nmemb) {
		return NULL;
	}
	
	total = nmemb * size;
	p = malloc(total);
	if (!p) {
		return NULL;
	}
	
	// recycled memory is not necessarily clean, so zero it here rather than
	// relying on fresh pages arriving zeroed
	memset(p, 0, total);
	return p;
}

#endif
