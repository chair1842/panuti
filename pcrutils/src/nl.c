/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

#define NL_WIDTH 6
#define NL_NUMSZ 32

static long start = 1;
static char sep = '\t';
static bool only_out0 = false;

static const char HELP[] =
	"nl - a pcrutils utility\n\n"
	"nl reads in0 and numbers its lines, writing them back out to every\n"
	"out stream with the number up front\n\n"
	"usage:\n"
	"  nl [-v start] [-s sep] [-o]\n\n"
	"args:\n"
	"  -v start - the first number, 1 if not given\n"
	"  -s sep - the separator after the number, a tab if not given\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"the number is right-aligned to six columns before the separator\n\n"
	"nl reads a stream, so it takes no paths, try in a > nl\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: nl: unexpected argument '%s'\n", a);
			printf("nl reads in stream 0, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-o") == 0) {
			only_out0 = true;
		} else if (strcmp(a, "-v") == 0 || strcmp(a, "-s") == 0) {
			bool is_start = (a[1] == 'v');

			if (i + 1 >= argc) {
				printf("pcrutils: nl: %s needs an argument\n", a);
				return -1;
			}

			const char* value = argv[++i];

			if (is_start) {
				bool ok;
				start = pcr_parse_long(value, &ok);
				if (!ok) {
					printf("pcrutils: nl: '%s' is not a number\n", value);
					return -1;
				}
			} else {
				if (value[1] != '\0') {
					printf("pcrutils: nl: -s must be a single character\n");
					return -1;
				}
				sep = value[0];
			}
		} else {
			printf("pcrutils: nl: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: nl: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: nl: there are no out streams to write to\n");
		return -1;
	}

	pcr_lines_t lines;
	if (pcr_stream_lines(0, &lines) != 0) {
		printf("pcrutils: nl: could not read the in stream\n");
		return -1;
	}

	char num[NL_NUMSZ];
	long n = start;
	int failed = 0;

	for (size_t i = 0; i < lines.n; i++) {
		int nn = pcr_uformat((unsigned long)n, 10, num);
		if (nn <= 0) {
			failed = 1;
			break;
		}

		for (int pad = nn; pad < NL_WIDTH; pad++) {
			num[pad] = ' ';
		}

		if (only_out0) {
			if (pcr_write_all(0, num, NL_WIDTH) != 0 ||
			    pcr_write_all(0, &sep, 1) != 0 ||
			    pcr_write_all(0, lines.v[i], strlen(lines.v[i])) != 0) {
				failed = 1;
				break;
			}
		} else {
			pcr_buf_t out = {0};

			if (pcr_buf_append(&out, num, NL_WIDTH) != 0 ||
			    pcr_buf_append(&out, &sep, 1) != 0 ||
			    pcr_buf_append(&out, lines.v[i], strlen(lines.v[i])) != 0 ||
			    pcr_output(counts[1], out.data, out.len, 0) != 0) {
				pcr_buf_free(&out);
				failed = 1;
				break;
			}

			pcr_buf_free(&out);
		}

		n++;
	}

	pcr_lines_free(&lines);

	if (failed) {
		printf("pcrutils: nl: could not write to the out streams\n");
		return -1;
	}

	return 0;
}