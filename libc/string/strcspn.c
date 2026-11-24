/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

size_t strcspn(const char* s, const char* reject) {
	size_t n = 0;
	for (; s[n]; n++) {
		for (const char* r = reject; *r; r++) {
			if (*r == s[n]) {
				return n;
			}
		}
	}
	
	return n;
}
