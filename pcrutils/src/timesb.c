/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"timesb - a pcrutils utility\n\n"
	"timesb prints the time since boot in centiseconds by default\n\n"
	"args (only the first arg is considered):\n"
	"  -h - prints this help message\n"
	"  -s - converts the time since boot to seconds\n"
	"  -l - converts the time since boot to milliseconds\n"
	"  -m - converts the time since boot to minutes\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	if (argc > 1) {
		if (strcmp(argv[1], "-s") == 0) {
			printf("%d\n", panutisysf_timesb() / 100);
			return 0;
		} else if (strcmp(argv[1], "-l") == 0) {
			printf("%d\n", panutisysf_timesb() * 10);
			return 0;
		} else if (strcmp(argv[1], "-m") == 0) {
			printf("%d\n", panutisysf_timesb() / (100 * 60));
			return 0;
		}
	} else {
		printf("%d\n", panutisysf_timesb());
		return 0;
	}
}