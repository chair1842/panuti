/* SPDX-License-Identifier: BSD-3-Clause */

#include <strings.h>

static int lower(int c) {
	if (c >= 'A' && c <= 'Z') {
		return c + ('a' - 'A');
	}
	
	return c;
}

int strcasecmp(const char* a, const char* b) {
	while (*a && *b) {
		int ca = lower((unsigned char)*a);
		int cb = lower((unsigned char)*b);
		if (ca != cb) {
			return ca - cb;
		}
		
		a++;
		b++;
	}
	
	return lower((unsigned char)*a) - lower((unsigned char)*b);
}
