/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdlib.h>
#include <string.h>

char* strdup(const char* s) {
	size_t len = strlen(s) + 1;
	char* p = malloc(len);
	if (!p) {
		return NULL;
	}
	
	memcpy(p, s, len);
	return p;
}
