/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

char* strrchr(const char* s, int c) {
	char ch = (char)c;
	const char* last = NULL;
	for (;;) {
		if (*s == ch) {
			last = s;
		}
		
		if (*s == '\0') {
			return (char*)last;
		}
		
		s++;
	}
}
