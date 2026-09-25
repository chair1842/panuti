/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/syscall/syscallsf.h>

int main(int argc, char** argv) {
	if (argc > 1) {
		if (strcmp(argv[1], "-h") == 0) {
			printf("sleep - a pcrutils utility\n\n");
			printf("sleep for a given number of seconds\n\n");
			printf("args (only the first argument is considered):\n");
			printf("  -h - prints this help message\n");
			printf("  <seconds> - number of seconds (whole, non-negative)\n");

			return 1;
		}
	}

	if (argc < 2) {
		printf("sleep: missing operand\n");
		return -1;
	}

	long seconds = 0;
	for (const char* p = argv[1]; *p; p++) {
		if (*p < '0' || *p > '9') {
			printf("sleep: invalid time interval '%s'\n", argv[1]);
			return -1;
		}
		seconds = seconds * 10 + (*p - '0');
	}

	int32_t start = panutisysf_timesb();
	if (start < 0) {
		printf("sleep: cannot read the clock\n");
		return -1;
	}

	long target = (long)start + seconds * 100;
	while ((long)panutisysf_timesb() < target) {
		panutisysf_yield();
	}

	return 0;
}