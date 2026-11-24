/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

char* stpcpy(char* restrict dest, const char* restrict src) {
	while ((*dest = *src) != '\0') {
		dest++;
		src++;
	}
	
	return dest;
}
