/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"tsb - a pcrutils utility\n\n"
	"tsb prints the time since boot in centiseconds by default\n\n"
	"args:\n"
	"  -h - prints this help message\n"
	"  -s - converts the time since boot to seconds\n"
	"  -l - converts the time since boot to milliseconds\n"
	"  -m - converts the time since boot to minutes\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	long centis;
	if (pcr_uptime_centis(&centis) != 0) {
		printf("tsb: cannot read /dvc/uptime\n");
		return -1;
	}

	if (pcr_option_provided(argc, argv, "-s")) {
		printf("%ld\n", centis / 100);
	} else if (pcr_option_provided(argc, argv, "-l")) {
		printf("%ld\n", centis * 10);
	} else if (pcr_option_provided(argc, argv, "-m")) {
		printf("%ld\n", centis / (100 * 60));
	} else {
		printf("%ld\n", centis);
	}

	return 0;
}