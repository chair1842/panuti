/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"sleep - a pcrutils utility\n\n"
	"sleep for a given number of seconds\n\n"
	"args (only the first argument is considered):\n"
	"  -h - prints this help message\n"
	"  <seconds> - number of seconds (whole, non-negative)\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
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

	long start;
	if (pcr_uptime_centis(&start) != 0) {
		printf("sleep: cannot read /dvc/uptime\n");
		return -1;
	}

	long target = start + seconds * 100;
	for (;;) {
		long now;
		if (pcr_uptime_centis(&now) != 0) {
			printf("sleep: cannot read /dvc/uptime\n");
			return -1;
		}
		
		if (now >= target) {
			break;
		}
		
		panutisysf_yield();
	}

	return 0;
}