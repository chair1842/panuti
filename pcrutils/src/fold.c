/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

static int width = 80;
static bool only_out0 = false;

static const char HELP[] =
	"fold - a pcrutils utility\n\n"
	"fold wraps whatever runs through it at a fixed number of columns,\n"
	"breaking the long lines without touching the short ones\n\n"
	"usage:\n"
	"  fold [-w <n>] [-o]\n\n"
	"args:\n"
	"  -w <n> - the width to wrap at, in characters, default 80\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"fold reads a stream, so it takes no paths, try in a > fold\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	int first = argc;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			first = i;
			break;
		}

		if (strcmp(a, "-o") == 0) {
			only_out0 = true;
		} else if (strcmp(a, "-w") == 0) {
			if (i + 1 >= argc) {
				printf("pcrutils: fold: -w needs a width\n");
				return -1;
			}

			bool ok;
			long w = pcr_parse_long(argv[++i], &ok);
			if (!ok || w < 1) {
				printf("pcrutils: fold: '%s' is not a width\n", argv[i]);
				return -1;
			}

			width = (int)w;
		} else {
			printf("pcrutils: fold: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	if (first != argc) {
		printf("pcrutils: fold: unexpected argument '%s'\n", argv[first]);
		printf("fold reads in stream 0, it takes no paths\n");
		return -1;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: fold: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: fold: there are no out streams to write to\n");
		return -1;
	}

	pcr_buf_t out = {0};
	unsigned int col = 0;
	int failed = 0;

	char buf[128];

	for (;;) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: fold: could not read the in stream\n");
			failed = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		for (int i = 0; i < n; i++) {
			char c = buf[i];

			// a line that reaches the limit gets broken, unless the byte
			// breaking it is the line's own newline
			if (col == (unsigned int)width && c != '\n') {
				if (pcr_buf_append(&out, "\n", 1) != 0) {
					failed = 1;
					break;
				}
				col = 0;
			}

			if (pcr_buf_append(&out, &c, 1) != 0) {
				failed = 1;
				break;
			}

			if (c == '\n') {
				col = 0;
			} else {
				col++;
			}
		}

		if (failed) {
			break;
		}
	}

	if (!failed && out.len > 0 && pcr_output(counts[1], out.data, out.len, only_out0) != 0) {
		printf("pcrutils: fold: could not write to the out streams\n");
		failed = 1;
	}

	pcr_buf_free(&out);
	return failed ? -1 : 0;
}