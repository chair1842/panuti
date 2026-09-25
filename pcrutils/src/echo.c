/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>

int main(int argc, char** argv) {
	for (int i = 1; i <= (argc - 1); i++) {
		printf("%s", argv[i]);
	}

	printf("\n");
	
	return 0;
}