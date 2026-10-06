/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

static bool only_out0 = false;

static const char HELP[] =
	"paste - a pcrutils utility\n\n"
	"paste takes the lines of in0 and in1, and writes them to every out\n"
	"stream as one line, the two sides put together with a tab between\n"
	"them. the longer side runs on by itself once the shorter one is done\n\n"
	"usage:\n"
	"  paste [-o]\n\n"
	"args:\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"with only one in stream wired in, paste just copies its lines out\n"
	"paste reads streams, so it takes no paths, try in a > paste < in b\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: paste: unexpected argument '%s'\n", a);
			printf("paste reads in streams 0 and 1, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-o") != 0) {
			printf("pcrutils: paste: invalid option '%s'\n", a + 1);
			return -1;
		}

		only_out0 = true;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: paste: there are no in streams to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: paste: there are no out streams to write to\n");
		return -1;
	}

	pcr_lines_t a;
	pcr_lines_t b;

	if (pcr_stream_lines(0, &a) != 0) {
		printf("pcrutils: paste: could not read in stream 0\n");
		return -1;
	}

	if (counts[0] < 2) {
		b = (pcr_lines_t){0};
	} else if (pcr_stream_lines(1, &b) != 0) {
		printf("pcrutils: paste: could not read in stream 1\n");
		pcr_lines_free(&a);
		return -1;
	}

	size_t upper = (a.n > b.n) ? a.n : b.n;
	int failed = 0;

	for (size_t i = 0; i < upper; i++) {
		pcr_buf_t out = {0};
		size_t left = (a.n > i) ? strlen(a.v[i]) : 0;
		size_t right = (b.n > i) ? strlen(b.v[i]) : 0;

		if (left > 0 && pcr_buf_append(&out, a.v[i], left) != 0) {
			goto oom;
		}

		if (left > 0 && right > 0 && pcr_buf_append(&out, "\t", 1) != 0) {
			goto oom;
		}

		if (right > 0 && pcr_buf_append(&out, b.v[i], right) != 0) {
			goto oom;
		}

		if (pcr_output(counts[1], out.data, out.len, only_out0) != 0) {
			pcr_buf_free(&out);
			failed = 1;
			break;
		}

		pcr_buf_free(&out);
		continue;

	oom:
		pcr_buf_free(&out);
		failed = 1;
		break;
	}

	pcr_lines_free(&a);
	pcr_lines_free(&b);

	if (failed) {
		printf("pcrutils: paste: could not write to the out streams\n");
		return -1;
	}

	return 0;
}