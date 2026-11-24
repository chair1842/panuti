/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

char* strpbrk(const char* s, const char* accept) {
	for (; *s; s++) {
		for (const char* a = accept; *a; a++) {
			if (*a == *s) {
				return (char*)s;
			}
		}
	}
	
	return NULL;
}
