#include <string.h>
#include <stddef.h>

int strncmp(const char* s1, const char* s2, size_t n) {
	while (n > 0) {
		unsigned char c1 = (unsigned char)*s1;
		unsigned char c2 = (unsigned char)*s2;

		if (c1 != c2) {
			return c1 - c2;
		}

		if (c1 == '\0') {
			return 0;
		}

		s1++;
		s2++;
		n--;
	}

	return 0;
}