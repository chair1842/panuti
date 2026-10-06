/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

#define CUT_MAXRANGES 64

typedef struct {
	long lo;
	long hi;
} cut_range_t;

static char delim = '\t';
static cut_range_t ranges[CUT_MAXRANGES];
static int no_ranges = 0;
static bool only_out0 = false;

static const char HELP[] =
	"cut - a pcrutils utility\n\n"
	"cut breaks in0 into fields, and writes the chosen ones back out to every out stream\n"
	"re-joining them with the delimiter\n\n"
	"usage:\n"
	"  cut [-d delim] -f fields [-o]\n\n"
	"args:\n"
	"  -d delim - the field separator, a tab if not given\n"
	"  -f fields - the fields to keep, as in 2 or 1,3 or 2-5 or 1,3-5\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"fields are counted from 1, and a line with no delimiter in it is one whole field\n\n"
	"cut reads a stream, so it takes no paths, try in a > cut\n";

static bool parse_number(const char* s, long* out) {
	bool ok;
	*out = pcr_parse_long(s, &ok);
	return ok;
}

static int add_range(long lo, long hi) {
	if (no_ranges >= CUT_MAXRANGES || lo < 1 || hi < lo) {
		return -1;
	}

	ranges[no_ranges].lo = lo;
	ranges[no_ranges].hi = hi;
	no_ranges++;
	return 0;
}

static int parse_field_spec(const char* spec) {
	const char* p = spec;

	for (;;) {
		const char* comma = pcr_strchr(p, ',');

		size_t toklen = comma ? (size_t)(comma - p) : strlen(p);

		char* tok = malloc(toklen + 1);
		if (!tok) {
			return -1;
		}

		memcpy(tok, p, toklen);
		tok[toklen] = '\0';

		const char* dash = pcr_strchr(tok, '-');
		long lo, hi;

		if (dash && dash[1] != '\0' && dash != tok) {
			bool lo_ok, hi_ok;
			lo = pcr_parse_long(tok, &lo_ok);
			hi = pcr_parse_long(dash + 1, &hi_ok);

			if (!lo_ok || !hi_ok || add_range(lo, hi) != 0) {
				free(tok);
				return -1;
			}
		} else if (!dash && toklen > 0) {
			if (!parse_number(tok, &lo) || add_range(lo, lo) != 0) {
				free(tok);
				return -1;
			}
		} else {
			free(tok);
			return -1;
		}

		free(tok);

		if (!comma) {
			return 0;
		}

		p = comma + 1;
	}
}

static bool field_selected(long k) {
	for (int i = 0; i < no_ranges; i++) {
		if (k >= ranges[i].lo && k <= ranges[i].hi) {
			return true;
		}
	}

	return false;
}

static int cut_line(int no_streams, int only_out0, const char* line) {
	pcr_buf_t out = {0};
	size_t len = strlen(line);
	size_t i = 0;
	long field = 1;
	bool want_sep = false;

	for (;;) {
		// the run up to the next delimiter, or the line's end. consecutive
		// delimiters make an empty field of their own.
		size_t start = i;
		while (i < len && line[i] != delim) {
			i++;
		}

		if (field_selected(field)) {
			if (want_sep) {
				if (pcr_buf_append(&out, &delim, 1) != 0) {
					pcr_buf_free(&out);
					return -1;
				}
			}

			if (pcr_buf_append(&out, line + start, i - start) != 0) {
				pcr_buf_free(&out);
				return -1;
			}

			want_sep = true;
		}

		if (i >= len) {
			break;
		}

		field++;
		i++;

		// a delimiter right at the end leaves one empty field behind it
		if (i == len && field_selected(field)) {
			if (want_sep) {
				if (pcr_buf_append(&out, &delim, 1) != 0) {
					pcr_buf_free(&out);
					return -1;
				}
			}
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

	int i = 1;
	for (; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: cut: unexpected argument '%s'\n", a);
			printf("cut reads in stream 0, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-o") == 0) {
			only_out0 = true;
		} else if (strcmp(a, "-d") == 0 || strcmp(a, "-f") == 0) {
			bool is_delim = (a[1] == 'd');

			if (i + 1 >= argc) {
				printf("pcrutils: cut: %s needs an argument\n", a);
				return -1;
			}

			const char* value = argv[++i];

			if (is_delim && value[1] != '\0') {
				printf("pcrutils: cut: -d must be a single character\n");
				return -1;
			}

			if (is_delim) {
				delim = value[0];
			} else if (parse_field_spec(value) != 0) {
				printf("pcrutils: cut: '%s' is not a field list\n", value);
				return -1;
			}
		} else {
			printf("pcrutils: cut: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	if (no_ranges == 0) {
		printf("pcrutils: cut: no fields to cut (-f is needed)\n");
		return -1;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: cut: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: cut: there are no out streams to write to\n");
		return -1;
	}

	pcr_lines_t lines;
	if (pcr_stream_lines(0, &lines) != 0) {
		printf("pcrutils: cut: could not read the in stream\n");
		return -1;
	}

	int failed = 0;

	for (size_t k = 0; k < lines.n; k++) {
		if (cut_line(counts[1], only_out0, lines.v[k]) != 0) {
			failed = 1;
			break;
		}
	}

	pcr_lines_free(&lines);

	if (failed) {
		printf("pcrutils: cut: could not write to the out streams\n");
		return -1;
	}

	return 0;
}