#include <string.h>

void* memchr(const void* buf, int ch, size_t n) {
	const unsigned char* p = (const unsigned char*)buf;
	unsigned char c = (unsigned char)ch;

	for (size_t i = 0; i < n; i++) {
		if (p[i] == c) {
			return (void*)(p + i);
		}
	}
	return NULL;
}