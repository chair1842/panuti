/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}
