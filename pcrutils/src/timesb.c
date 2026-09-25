/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/syscall/syscallsf.h>

int main(int argc, char** argv) {
	if (argc > 1) {
		if (strcmp(argv[1], "-h") == 0) {
			printf("timesb - a pcrutils utility\n\n");
			printf("timesb prints the time since boot in centiseconds by default\n\n");
			printf("args (only the first arg is considered):\n");
			printf("  -h - prints this help message\n");
			printf("  -s - converts the time since boot to seconds\n");
			printf("  -mm - converts the time since boot to milliseconds\n");
			printf("  -m - converts the time scince boot to minutes\n");

			return 1;
		} else if (strcmp(argv[1], "-s") == 0) {
			printf("%d\n", panutisysf_timesb() / 100);
			return 2;
		} else if (strcmp(argv[1], "-mm") == 0) {
			printf("%d\n", panutisysf_timesb() * 10);
			return 3;
		} else if (strcmp(argv[1], "-m") == 0) {
			printf("%d\n", panutisysf_timesb() / (100 * 60));
			return 4;
		}
	} else {
		printf("%d\n", panutisysf_timesb());
		return 0;
	}
}