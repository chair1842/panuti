/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <panuti/stat.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"nexist - a pcrutils utility\n\n"
	"nexist checks if a node in the registry exists\n"
	"it prints \"true\" if it exists, \"false\" otherwise\n\n"
	"args (only the first argument is considered):\n"
	"  -h - prints this help message\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	if (argc < 2) {
		printf("pcrutils: nexist: no path given\n");
		return -1;
	}

	printf("%s\n", nexist(argv[1]) ? "true" : "false");

	return 0;
}