/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

size_t strnlen(const char* s, size_t maxlen) {
	size_t n = 0;
	while (n < maxlen && s[n] != '\0') {
		n++;
	}
	
	return n;
}
