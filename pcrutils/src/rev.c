/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

static bool only_out0 = false;

static const char HELP[] =
	"rev - a pcrutils utility\n\n"
	"rev reads in0 and writes its lines back out to every out stream,\n"
	"each one with its characters reversed. the line order is kept, and\n"
	"a newline on the line is left at the end where it was\n\n"
	"usage:\n"
	"  rev [-o]\n\n"
	"args:\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"rev reads a stream, so it takes no paths, try in a > rev\n";

static int rev_line(int no_streams, int only_out0, const char* line) {
	size_t len = strlen(line);

	// the newline, when there is one, stays at the end of the line
	size_t body = (len > 0 && line[len - 1] == '\n') ? len - 1 : len;

	pcr_buf_t out = {0};

	if (pcr_buf_append(&out, line + body, len - body) != 0) {
		pcr_buf_free(&out);
		return -1;
	}

	for (size_t i = body; i > 0; i--) {
		if (pcr_buf_append(&out, line + i - 1, 1) != 0) {
			pcr_buf_free(&out);
			return -1;
		}
	}

	int rc = pcr_output(no_streams, out.data, out.len, only_out0);
	pcr_buf_free(&out);
	return rc;
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: rev: unexpected argument '%s'\n", a);
			printf("rev reads in stream 0, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-o") != 0) {
			printf("pcrutils: rev: invalid option '%s'\n", a + 1);
			return -1;
		}

		only_out0 = true;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: rev: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: rev: there are no out streams to write to\n");
		return -1;
	}

	pcr_lines_t lines;
	if (pcr_stream_lines(0, &lines) != 0) {
		printf("pcrutils: rev: could not read the in stream\n");
		return -1;
	}

	int failed = 0;

	for (size_t i = 0; i < lines.n; i++) {
		if (rev_line(counts[1], only_out0, lines.v[i]) != 0) {
			failed = 1;
			break;
		}
	}

	pcr_lines_free(&lines);

	if (failed) {
		printf("pcrutils: rev: could not write to the out streams\n");
		return -1;
	}

	return 0;
}