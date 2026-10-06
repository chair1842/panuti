/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

static bool only_out0 = false;

static const char HELP[] =
	"tac - a pcrutils utility\n\n"
	"tac reads in0 to the end and writes its lines back out to every\n"
	"out stream, last line first. it is rev's opposite: the lines keep\n"
	"their characters, only their order is turned around\n\n"
	"usage:\n"
	"  tac [-o]\n\n"
	"args:\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"tac reads a stream, so it takes no paths, try in a > tac\n"
	"the whole input has to fit in memory, the way split does\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: tac: unexpected argument '%s'\n", a);
			printf("tac reads in stream 0, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-o") != 0) {
			printf("pcrutils: tac: invalid option '%s'\n", a + 1);
			return -1;
		}

		only_out0 = true;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: tac: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: tac: there are no out streams to write to\n");
		return -1;
	}

	pcr_lines_t lines;
	if (pcr_stream_lines(0, &lines) != 0) {
		printf("pcrutils: tac: could not read the in stream\n");
		return -1;
	}

	int failed = 0;

	for (size_t i = lines.n; i > 0; i--) {
		if (pcr_output(counts[1], lines.v[i - 1], strlen(lines.v[i - 1]), only_out0) != 0) {
			failed = 1;
			break;
		}
	}

	pcr_lines_free(&lines);

	if (failed) {
		printf("pcrutils: tac: could not write to the out streams\n");
		return -1;
	}

	return 0;
}