/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/stat.h>

int main(int argc, char** argv) {
	if (argc > 1 && strcmp(argv[1], "-h") == 0) {
		printf("nexist - a pcrutils utility\n\n");
		printf("nexist checks if a node in the registry exists\n");
		printf("it prints \"true\" if it exists, \"false\" otherwise\n\n");
		printf("args (only the first argument is considered):\n");
		printf("  -h - prints this help message\n");

		return 0;
	}

	if (argc < 2) {
		printf("pcrutils: nexist: no path given\n");
		return -1;
	}

	printf("%s\n", nexist(argv[1]) ? "true" : "false");

	return 0;
}