/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

char* strtok_r(char* str, const char* delim, char** saveptr) {
	char* s = str ? str : *saveptr;
	if (!s) {
		return NULL;
	}

	s += strspn(s, delim);
	if (*s == '\0') {
		*saveptr = NULL;
		return NULL;
	}

	char* end = s + strcspn(s, delim);
	if (*end == '\0') {
		*saveptr = NULL;
	} else {
		*end = '\0';
		*saveptr = end + 1;
	}
	
	return s;
}
