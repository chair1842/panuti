/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

char* strstr(const char* haystack, const char* needle) {
	if (*needle == '\0') {
		return (char*)haystack;
	}

	for (; *haystack; haystack++) {
		if (*haystack != *needle) {
			continue;
		}

		const char* h = haystack + 1;
		const char* n = needle + 1;
		while (*n && *h == *n) {
			h++;
			n++;
		}
		
		if (*n == '\0') {
			return (char*)haystack;
		}
	}

	return NULL;
}
