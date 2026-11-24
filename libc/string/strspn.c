/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

size_t strspn(const char* s, const char* accept) {
	size_t n = 0;
	for (; s[n]; n++) {
		const char* a = accept;
		while (*a && *a != s[n]) {
			a++;
		}
		
		if (*a == '\0') {
			break;
		}
	}
	return n;
}
