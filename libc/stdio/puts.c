/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>

int puts(const char* string) {
	return printf("%s\n", string);
}
