/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <panuti/stream.h>

#define GREP_LINESZ 256
#define GREP_READSZ 128

static bool only_out0 = false;

static char* line;
static size_t line_cap;

static int line_reserve(size_t need) {
	if (need <= line_cap) {
		return 0;
	}

	size_t want = line_cap ? line_cap : GREP_LINESZ;

	while (want < need) {
		if (want > (size_t)-1 / 2) {
			return -1;
		}
		want *= 2;
	}

	char* p = realloc(line, want);
	if (!p) {
		return -1;
	}

	line = p;
	line_cap = want;
	
	return 0;
}

static void print_help(void) {
	printf("grep - a pcrutils utility\n\n");
	printf("grep reads in0, and outputs every line that holds the word\n");
	printf("it is given. a line is copied out as it came in, newline and all\n\n");
	printf("usage:\n");
	printf("  grep <word>\n\n");
	printf("args:\n");
	printf("  -h - prints this help message\n");
	printf("  -o - only output to out0\n");
}

// streams can accept less than we give them (pipes do), so keep writing
// the rest until everything is out or the stream stops making progress
static int write_all(int stream, const char* buf, size_t len) {
	size_t off = 0;

	while (off < len) {
		int w = stream_write(stream, buf + off, len - off);
		if (w <= 0) {
			return -1;
		}

		off += (size_t)w;
	}

	return 0;
}

static int output(int no_streams, const char* buf, size_t len) {
	int streams = only_out0 ? 1 : no_streams;
	int failed = 0;

	for (int s = 0; s < streams; s++) {
		if (write_all(s, buf, len) != 0) {
			failed = 1;
		}
	}

	return failed ? -1 : 0;
}

// this libc has no strstr, so slide the needle over the haystack
static bool contains(const char* hay, size_t hlen, const char* needle, size_t nlen) {
	if (nlen == 0) {
		return true;
	}

	if (nlen > hlen) {
		return false;
	}

	for (size_t i = 0; i + nlen <= hlen; i++) {
		if (memcmp(hay + i, needle, nlen) == 0) {
			return true;
		}
	}

	return false;
}

int main(int argc, char** argv) {
	int first = argc;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			first = i;
			break;
		}

		for (int j = 1; a[j]; j++) {
			if (a[j] == 'h') {
				print_help();
				return 1;
			} else if (a[j] == 'o') {
				only_out0 = true;
			} else {
				printf("pcrutils: grep: invalid option '%c'\n", a[j]);
				return -1;
			}
		}
	}

	if (first == argc) {
		printf("pcrutils: grep: no word given\n");
		return -1;
	}

	if (first + 1 != argc) {
		printf("pcrutils: grep: only one word can be looked for\n");
		return -1;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: grep: there are no in streams to read from\n");
		return -1;
	}

	int no_streams = counts[1];
	if (no_streams < 1) {
		printf("pcrutils: grep: there are no out streams to write to\n");
		return -1;
	}

	const char* needle = argv[first];
	size_t nlen = strlen(needle);

	int failed = 0;
	size_t have = 0;
	char buf[GREP_READSZ];

	for (;;) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: grep: could not read the in stream\n");
			failed = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		for (int i = 0; i < n; ) {
			// stop at the newline, or take the rest of the chunk
			size_t take = (size_t)n - (size_t)i;

			for (size_t k = 0; k < take; k++) {
				if (buf[i + k] == '\n') {
					take = k + 1;
					break;
				}
			}

			if (line_reserve(have + take) != 0) {
				printf("pcrutils: grep: out of memory\n");
				failed = 1;
				break;
			}

			memcpy(line + have, buf + i, take);
			have += take;
			i += (int)take;

			if (line[have - 1] == '\n') {
				if (contains(line, have, needle, nlen) &&
				    output(no_streams, line, have) != 0) {
					printf("pcrutils: grep: could not write to the out streams\n");
					failed = 1;
				}

				have = 0;
			}

			if (failed) {
				break;
			}
		}

		if (failed) {
			break;
		}
	}

	// a last line with no newline of its own is still a line
	if (!failed && have > 0 && contains(line, have, needle, nlen)) {
		if (output(no_streams, line, have) != 0) {
			printf("pcrutils: grep: could not write to the out streams\n");
			failed = 1;
		}
	}

	free(line);
	line = NULL;
	line_cap = 0;

	return failed ? -1 : 0;
}
