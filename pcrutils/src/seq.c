/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

#define SEQ_NUMSZ 32

static const char HELP[] =
	"seq - a pcrutils utility\n\n"
	"seq prints a run of numbers, one to a line, to every out stream\n\n"
	"usage:\n"
	"  seq last\n"
	"  seq first last\n"
	"  seq first step last\n\n"
	"args:\n"
	"  -h - prints this help message\n\n"
	"the numbers are whole and non-negative. with no step given it counts\n"
	"up by one, and a negative step counts down. the bounds stop the run\n"
	"before the numbers would overflow\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	int first = argc;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || argv[i][1] != '\0') {
			first = i;
			break;
		}

		printf("pcrutils: seq: invalid option '%s'\n", argv[i] + 1);
		return -1;
	}

	if (first == argc) {
		printf("pcrutils: seq: no end of the run given\n");
		return -1;
	}

	long a = 1;
	long step = 1;
	long last;

	int remaining = argc - first;

	if (remaining > 3) {
		printf("pcrutils: seq: too many arguments\n");
		return -1;
	}

	bool ok;
	last = pcr_parse_long(argv[first], &ok);
	if (!ok) {
		printf("pcrutils: seq: '%s' is not a number\n", argv[first]);
		return -1;
	}

	if (remaining >= 2) {
		a = pcr_parse_long(argv[first + 1], &ok);
		if (!ok) {
			printf("pcrutils: seq: '%s' is not a number\n", argv[first + 1]);
			return -1;
		}
	}

	if (remaining >= 3) {
		step = pcr_parse_long(argv[first + 2], &ok);
		if (!ok) {
			printf("pcrutils: seq: '%s' is not a number\n", argv[first + 2]);
			return -1;
		}
	}

	if (step == 0) {
		printf("pcrutils: seq: the step cannot be zero\n");
		return -1;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[1] < 1) {
		printf("pcrutils: seq: there are no out streams to write to\n");
		return -1;
	}

	char num[SEQ_NUMSZ];
	int failed = 0;

	long v = a;

	for (long step_up = step > 0; step_up ? v <= last : v >= last; ) {
		int n = pcr_uformat((unsigned long)v, 10, num);
		if (n <= 0) {
			failed = 1;
			break;
		}

		if (pcr_output(counts[1], num, (size_t)n, 0) != 0 ||
		    pcr_output(counts[1], "\n", 1, 0) != 0) {
			failed = 1;
			break;
		}

		long next = v + step;

		// a step that wraps past either end of the long has gone wrong
		if (step_up ? next <= v : next >= v) {
			break;
		}

		v = next;
	}

	if (failed) {
		printf("pcrutils: seq: could not write to the out streams\n");
		return -1;
	}

	return 0;
}