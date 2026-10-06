/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

static bool col1 = true;
static bool col1_given = false;
static bool col2 = true;
static bool col2_given = false;
static bool col3 = true;
static bool col3_given = false;
static bool only_out0 = false;

static const char HELP[] =
	"comm - a pcrutils utility\n\n"
	"comm compares the lines of in0 with the lines of in1, and writes\n"
	"them out to every out stream in three columns: lines only in in0,\n"
	"lines only in in1, and lines in both, the columns set off by tabs\n\n"
	"usage:\n"
	"  comm [-1] [-2] [-3] [-o]\n\n"
	"args:\n"
	"  -1 - do not print lines only in in0\n"
	"  -2 - do not print lines only in in1\n"
	"  -3 - do not print lines in both\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"both in streams have to be sorted, the way sort orders them\n\n"
	"comm reads streams, so it takes no paths, try in a > comm < in b\n";

static int emit_col(int no_streams, int only_out0, const char* prefix, const char* line) {
	size_t plen = strlen(prefix);
	size_t llen = strlen(line);

	if (only_out0) {
		if (pcr_write_all(0, prefix, plen) != 0 ||
		    pcr_write_all(0, line, llen) != 0) {
			return -1;
		}
		return 0;
	}

	pcr_buf_t out = {0};

	if (pcr_buf_append(&out, prefix, plen) != 0 ||
	    pcr_buf_append(&out, line, llen) != 0) {
		pcr_buf_free(&out);
		return -1;
	}

	int rc = pcr_output(no_streams, out.data, out.len, 0);
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
			printf("pcrutils: comm: unexpected argument '%s'\n", a);
			printf("comm reads in streams 0 and 1, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-1") == 0) {
			col1 = false;
			col1_given = true;
		} else if (strcmp(a, "-2") == 0) {
			col2 = false;
			col2_given = true;
		} else if (strcmp(a, "-3") == 0) {
			col3 = false;
			col3_given = true;
		} else if (strcmp(a, "-o") == 0) {
			only_out0 = true;
		} else {
			printf("pcrutils: comm: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	// like comm, all three columns are on until one is given
	if (col1_given || col2_given || col3_given) {
		if (!col1_given) col1 = false;
		if (!col2_given) col2 = false;
		if (!col3_given) col3 = false;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 2) {
		printf("pcrutils: comm: need two in streams, one on in0 and one on in1\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: comm: there are no out streams to write to\n");
		return -1;
	}

	pcr_lines_t a;
	pcr_lines_t b;
	a = (pcr_lines_t){0};
	b = (pcr_lines_t){0};

	if (pcr_stream_lines(0, &a) != 0 || pcr_stream_lines(1, &b) != 0) {
		printf("pcrutils: comm: could not read the in streams\n");
		pcr_lines_free(&a);
		pcr_lines_free(&b);
		return -1;
	}

	int failed = 0;
	size_t i = 0;
	size_t j = 0;

	while (i < a.n || j < b.n) {
		int c;

		if (i >= a.n) {
			c = 1;
		} else if (j >= b.n) {
			c = -1;
		} else {
			c = strcmp(a.v[i], b.v[j]);
		}

		if (c < 0) {
			if (col1 && emit_col(counts[1], only_out0, "", a.v[i]) != 0) {
				failed = 1;
				break;
			}
			i++;
		} else if (c > 0) {
			if (col2 && emit_col(counts[1], only_out0, "\t", b.v[j]) != 0) {
				failed = 1;
				break;
			}
			j++;
		} else {
			if (col3 && emit_col(counts[1], only_out0, "\t\t", a.v[i]) != 0) {
				failed = 1;
				break;
			}
			i++;
			j++;
		}
	}

	pcr_lines_free(&a);
	pcr_lines_free(&b);

	if (failed) {
		printf("pcrutils: comm: could not write to the out streams\n");
		return -1;
	}

	return 0;
}