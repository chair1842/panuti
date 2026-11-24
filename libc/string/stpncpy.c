/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

char* stpncpy(char* restrict dest, const char* restrict src, size_t n) {
	size_t i = 0;
	for (; i < n && src[i] != '\0'; i++) {
		dest[i] = src[i];
	}
	
	char* end = dest + i;
	for (; i < n; i++) {
		dest[i] = '\0';
	}
	
	return end;
}
