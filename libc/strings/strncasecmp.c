/* SPDX-License-Identifier: BSD-3-Clause */

#include <strings.h>

static int lower(int c) {
	if (c >= 'A' && c <= 'Z') {
		return c + ('a' - 'A');
	}
	
	return c;
}

int strncasecmp(const char* a, const char* b, size_t n) {
	while (n > 0 && *a && *b) {
		int ca = lower((unsigned char)*a);
		int cb = lower((unsigned char)*b);
		if (ca != cb) {
			return ca - cb;
		}
		
		a++;
		b++;
		n--;
	}
	
	if (n == 0) {
		return 0;
	}
	
	return lower((unsigned char)*a) - lower((unsigned char)*b);
}
