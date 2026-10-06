/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

static bool reverse = false;
static bool numeric = false;
static bool only_out0 = false;

static const char HELP[] =
	"sort - a pcrutils utility\n\n"
	"sort reads in0 to the end, sorts its lines, and writes them out\n"
	"to every out stream\n\n"
	"usage:\n"
	"  sort [-r] [-n] [-o]\n\n"
	"args:\n"
	"  -r - sort in reverse order\n"
	"  -n - sort on the leading integer of each line\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"sort reads a stream, so it takes no paths, try in a > sort\n"
	"the whole input has to fit in memory, the way split does\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: sort: unexpected argument '%s'\n", a);
			printf("sort reads in stream 0, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-r") == 0) {
			reverse = true;
		} else if (strcmp(a, "-n") == 0) {
			numeric = true;
		} else if (strcmp(a, "-o") == 0) {
			only_out0 = true;
		} else {
			printf("pcrutils: sort: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: sort: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: sort: there are no out streams to write to\n");
		return -1;
	}

	pcr_lines_t lines;
	if (pcr_stream_lines(0, &lines) != 0) {
		printf("pcrutils: sort: could not read the in stream\n");
		return -1;
	}

	pcr_strs_sort(lines.v, lines.n, reverse, numeric);

	int failed = 0;

	for (size_t i = 0; i < lines.n; i++) {
		if (pcr_output(counts[1], lines.v[i], strlen(lines.v[i]), only_out0) != 0) {
			failed = 1;
			break;
		}
	}

	pcr_lines_free(&lines);

	if (failed) {
		printf("pcrutils: sort: could not write to the out streams\n");
		return -1;
	}

	return 0;
}