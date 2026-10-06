/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

#define TR_MAXSET 256

static bool only_out0 = false;

static const char HELP[] =
	"tr - a pcrutils utility\n\n"
	"tr translates the characters running through it. each character of\n"
	"set1 becomes the matching one of set2, and -d drops set1's characters\n"
	"instead\n\n"
	"usage:\n"
	"  tr [-o] set1 set2\n"
	"  tr -d [-o] set1\n\n"
	"a set is characters, ranges like a-z, and the escapes \\n \\t \\r\n"
	"\\v \\f \\\\\n\n"
	"args:\n"
	"  -d - delete the characters of set1\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"tr reads a stream, so it takes no paths, try in a > tr\n"
	"where set2 is shorter than set1, its last character is repeated\n";

static unsigned char map[256];
static bool del[256];
static bool do_delete = false;

static int set_char(char* out, const char** next, int* done) {
	const char* p = *next;

	if (*p == '\\') {
		switch (p[1]) {
			case 'n': *out = '\n'; *next = p + 2; return 0;
			case 't': *out = '\t'; *next = p + 2; return 0;
			case 'r': *out = '\r'; *next = p + 2; return 0;
			case 'v': *out = '\v'; *next = p + 2; return 0;
			case 'f': *out = '\f'; *next = p + 2; return 0;
			case '\\': *out = '\\'; *next = p + 2; return 0;
			case '\0': *out = '\\'; *next = p + 1; return 0;
			default: *out = p[1]; *next = p + 2; return 0;
		}
	}

	if (*p == '\0') {
		*next = p;
		return -1;
	}

	*out = *p;
	*next = p + 1;
	(void)done;
	return 0;
}

// expand a set into `into` (up to TR_MAXSET chars), returns its length. a
// "lo-hi" range needs both sides to be single characters.
static int set_expand(const char* spec, unsigned char* into) {
	int n = 0;
	const char* cur = spec;

	while (*cur) {
		unsigned char lo;

		if (set_char((char*)&lo, &cur, NULL) != 0) {
			return -1;
		}

		if (*cur == '-' && cur[1] != '\0') {
			unsigned char hi;

			if (set_char((char*)&hi, &cur, NULL) != 0) {
				return -1;
			}

			if (hi < lo) {
				return -1;
			}

			if ((size_t)(hi - lo) + (size_t)n > TR_MAXSET) {
				return -1;
			}

			for (unsigned int c = lo; c <= hi; c++) {
				into[n++] = (unsigned char)c;
			}
		} else {
			into[n++] = lo;
		}
	}

	return n;
}

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

		if (strcmp(a, "-d") == 0) {
			do_delete = true;
		} else if (strcmp(a, "-o") == 0) {
			only_out0 = true;
		} else {
			printf("pcrutils: tr: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	int need = do_delete ? 1 : 2;

	if (argc - first < need) {
		printf("pcrutils: tr: %s\n",
		       do_delete ? "a set to delete is needed" : "two sets are needed");
		return -1;
	}

	if (argc - first > need) {
		printf("pcrutils: tr: too many arguments\n");
		return -1;
	}

	for (int i = 0; i < 256; i++) {
		map[i] = (unsigned char)i;
		del[i] = false;
	}

	unsigned char set1[TR_MAXSET];
	unsigned char set2[TR_MAXSET];

	int n1 = set_expand(argv[first], set1);
	if (n1 < 0) {
		printf("pcrutils: tr: bad set '%s'\n", argv[first]);
		return -1;
	}

	if (!do_delete) {
		int n2 = set_expand(argv[first + 1], set2);
		if (n2 < 0) {
			printf("pcrutils: tr: bad set '%s'\n", argv[first + 1]);
			return -1;
		}

		unsigned char last = n2 > 0 ? set2[n2 - 1] : (unsigned char)0;

		for (int i = 0; i < n1; i++) {
			map[set1[i]] = (i < n2) ? set2[i] : last;
		}
	} else {
		for (int i = 0; i < n1; i++) {
			del[set1[i]] = true;
		}
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: tr: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: tr: there are no out streams to write to\n");
		return -1;
	}

	char buf[128];
	int failed = 0;

	for (;;) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: tr: could not read the in stream\n");
			return -1;
		}

		if (n == 0) {
			break;
		}

		char out[sizeof(buf)];
		size_t on = 0;

		for (int i = 0; i < n; i++) {
			unsigned char c = (unsigned char)buf[i];

			if (do_delete) {
				if (!del[c]) {
					out[on++] = (char)c;
				}
			} else {
				out[on++] = (char)map[c];
			}
		}

		if (on > 0 && pcr_output(counts[1], out, on, only_out0) != 0) {
			failed = 1;
			break;
		}
	}

	if (failed) {
		printf("pcrutils: tr: could not write to the out streams\n");
		return -1;
	}

	return 0;
}