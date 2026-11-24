/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

char* strncat(char* restrict dest, const char* restrict src, size_t n) {
	char* ret = dest;
	while (*dest) {
		dest++;
	}
	
	while (n > 0 && *src) {
		*dest++ = *src++;
		n--;
	}
	
	*dest = '\0';
	return ret;
}
