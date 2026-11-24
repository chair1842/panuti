/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

char* strchr(const char* s, int c) {
	char ch = (char)c;
	for (;;) {
		if (*s == ch) {
			return (char*)s;
		}
		
		if (*s == '\0') {
			return NULL;
		}
		
		s++;
	}
}
