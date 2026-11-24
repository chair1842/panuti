/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

char* strtok(char* str, const char* delim) {
	static char* save;
	return strtok_r(str, delim, &save);
}
