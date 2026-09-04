#include <string.h>

char* strcpy(char* dest, const char* src) {
	char* ret = dest;
	while ((*dest++ = *src++) != '\0') {
		// copies each byte including the final '\0', then stops
	}
	return ret;
}