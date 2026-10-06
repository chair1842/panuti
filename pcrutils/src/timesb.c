/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"timesb - a pcrutils utility\n\n"
	"timesb prints the time since boot in centiseconds by default\n\n"
	"args:\n"
	"  -h - prints this help message\n"
	"  -s - converts the time since boot to seconds\n"
	"  -l - converts the time since boot to milliseconds\n"
	"  -m - converts the time since boot to minutes\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	if (pcr_option_provided(argc, argv, "-s")) {
		printf("%d\n", panutisysf_timesb() / 100);
	} else if (pcr_option_provided(argc, argv, "-l")) {
		printf("%d\n", panutisysf_timesb() * 10);
	} else if (pcr_option_provided(argc, argv, "-m")) {
		printf("%d\n", panutisysf_timesb() / (100 * 60));
	} else {
		printf("%d\n", panutisysf_timesb());
	}

	return 0;
}