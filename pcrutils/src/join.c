/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

static bool only_out0 = false;

static const char HELP[] =
	"join - a pcrutils utility\n\n"
	"join matches the lines of in0 against the lines of in1 on their first field,"
	"and writes out one line per match to every out stream:\n"
	"the shared field, then the rest of the in0 line, then the rest of the in1 line,\n"
	"each part set off by a space\n\n"
	"usage:\n"
	"  join [-o]\n\n"
	"args:\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"a field is the run of the line before its first space or tab.\n"
	"both in streams are walked together, so both have to be sorted on their first field,\n"
	"the way sort orders them\n\n"
	"join reads streams, so it takes no paths, try in a > join < in b\n";

// the spot just past the first field, so the field is s[0..i) and the rest
// is s+i (possibly with its separating whitespace to skip)
static size_t first_field_end(const char* s) {
	size_t i = 0;
	while (s[i] && s[i] != ' ' && s[i] != '\t') {
		i++;
	}
	return i;
}

// the part of the line after the first field, with its leading and trailing
// whitespace (the newline among it) trimmed away
static const char* field_rest(const char* s, size_t* len_out) {
	size_t i = first_field_end(s);

	while (s[i] == ' ' || s[i] == '\t') {
		i++;
	}

	size_t end = strlen(s);
	while (end > i && (s[end - 1] == '\n' || s[end - 1] == ' ' || s[end - 1] == '\t')) {
		end--;
	}

	*len_out = end - i;
	return s + i;
}

// compare two first fields: equal length makes strncmp enough
static bool fields_equal(const char* la, size_t lla, const char* lb, size_t llb) {
	return lla == llb && strncmp(la, lb, lla) == 0;
}

static int emit_join(int no_streams, int only_out0,
                     const char* key, size_t key_len,
                     const char* ra, size_t ra_len,
                     const char* rb, size_t rb_len) {
	pcr_buf_t out = {0};

	if (pcr_buf_append(&out, key, key_len) != 0) {
		goto oom;
	}

	if (ra_len > 0 && (pcr_buf_append(&out, " ", 1) != 0 ||
	                   pcr_buf_append(&out, ra, ra_len) != 0)) {
		goto oom;
	}

	if (rb_len > 0 && (pcr_buf_append(&out, " ", 1) != 0 ||
	                   pcr_buf_append(&out, rb, rb_len) != 0)) {
		goto oom;
	}

	if (pcr_buf_append(&out, "\n", 1) != 0) {
		goto oom;
	}

	int rc = pcr_output(no_streams, out.data, out.len, only_out0);
	pcr_buf_free(&out);
	return rc;

oom:
	pcr_buf_free(&out);
	return -1;
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: join: unexpected argument '%s'\n", a);
			printf("join reads in streams 0 and 1, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-o") != 0) {
			printf("pcrutils: join: invalid option '%s'\n", a + 1);
			return -1;
		}

		only_out0 = true;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 2) {
		printf("pcrutils: join: need two in streams, one on in0 and one on in1\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: join: there are no out streams to write to\n");
		return -1;
	}

	pcr_lines_t a;
	pcr_lines_t b;
	a = (pcr_lines_t){0};
	b = (pcr_lines_t){0};

	if (pcr_stream_lines(0, &a) != 0 || pcr_stream_lines(1, &b) != 0) {
		printf("pcrutils: join: could not read the in streams\n");
		pcr_lines_free(&a);
		pcr_lines_free(&b);
		return -1;
	}

	int failed = 0;
	size_t i = 0;
	size_t j = 0;

	while (i < a.n && j < b.n) {
		size_t ka = first_field_end(a.v[i]);
		size_t kb = first_field_end(b.v[j]);

		size_t ra_len, rb_len;
		const char* ra = field_rest(a.v[i], &ra_len);
		const char* rb = field_rest(b.v[j], &rb_len);

		if (fields_equal(a.v[i], ka, b.v[j], kb)) {
			if (emit_join(counts[1], only_out0, a.v[i], ka, ra, ra_len, rb, rb_len) != 0) {
				failed = 1;
				break;
			}
			i++;
			j++;
		} else {
			size_t common = (ka < kb) ? ka : kb;
			int c = strncmp(a.v[i], b.v[j], common);

			if (c == 0) {
				c = (ka < kb) ? -1 : 1;
			}

			if (c < 0) {
				i++;
			} else {
				j++;
			}
		}
	}

	pcr_lines_free(&a);
	pcr_lines_free(&b);

	if (failed) {
		printf("pcrutils: join: could not write to the out streams\n");
		return -1;
	}

	return 0;
}