/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/stream.h>

#define HEAD_BUFSZ 128
#define HEAD_DEFAULT 10
#define HEAD_MAXCOUNT 1000000

static long want = HEAD_DEFAULT;
static bool only_out0 = false;

static void print_help(void) {
	printf("head - a pcrutils utility\n\n");
	printf("head reads in0, and copies the first lines of it out to every out stream\n");
	printf("a line is copied out as it came in, newline and all,\n");
	printf("and head stops once it has that many lines\n\n");
	printf("usage:\n");
	printf("  head [-n count]\n\n");
	printf("args:\n");
	printf("  -n count - copy the first count lines, 10 if not given\n");
	printf("  -o - only output to out0\n");
	printf("  -h - prints this help message\n\n");
	printf("the count can also be given the short way, as in head -5\n");
	printf("head reads a stream, so it takes no paths, try in a > head\n");
}

static bool parse_count(const char* s, long* out) {
	if (*s == '\0') {
		return false;
	}

	long v = 0;

	for (const char* p = s; *p; p++) {
		if (*p < '0' || *p > '9') {
			return false;
		}

		// the count only has to be sane, this keeps v from ever overflowing
		if (v > HEAD_MAXCOUNT) {
			return false;
		}

		v = (v * 10) + (*p - '0');
	}

	*out = v;
	return true;
}

// returns 0 to carry on, 1 when the help was asked for, -1 on a bad arg
static int parse_args(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: head: unexpected argument '%s'\n", a);
			printf("head reads in stream 0, it takes no paths\n");
			return -1;
		}

		for (int j = 1; a[j]; j++) {
			char c = a[j];

			if (c == 'h') {
				print_help();
				return 1;
			}

			if (c == 'o') {
				only_out0 = true;
				continue;
			}

			if (c == 'n') {
				// the count is either the rest of this arg or the next one
				const char* p = a + j + 1;

				if (*p == '\0') {
					if (i + 1 >= argc) {
						printf("pcrutils: head: -n needs a count\n");
						return -1;
					}

					p = argv[++i];
				}

				if (!parse_count(p, &want)) {
					printf("pcrutils: head: '%s' is not a count\n", p);
					return -1;
				}

				// whatever followed was the count
				break;
			}

			if (c >= '0' && c <= '9') {
				// the short way, as in head -5
				if (!parse_count(a + j, &want)) {
					printf("pcrutils: head: '%s' is not a count\n", a + j);
					return -1;
				}

				break;
			}

			printf("pcrutils: head: invalid option '%c'\n", c);
			return -1;
		}
	}

	return 0;
}

static int output(int no_streams, const char* data, size_t size) {
	if (only_out0) {
		if (stream_write(0, data, size) < 0) {
			printf("pcrutils: head: could not write to out stream 0\n");
			return -1;
		}

		return 0;
	}

	for (int o = 0; o < no_streams; o++) {
		if (stream_write(o, data, size) < 0) {
			printf("pcrutils: head: could not write to out stream %d\n", o);
			return -1;
		}
	}

	return 0;
}

int main(int argc, char** argv) {
	int parsed = parse_args(argc, argv);

	if (parsed != 0) {
		return parsed > 0 ? 0 : -1;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: head: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: head: there are no out streams to write to\n");
		return -1;
	}

	int no_streams = counts[1];
	char buf[HEAD_BUFSZ];
	long lines = 0;

	for (;;) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: head: could not read in stream 0\n");
			return -1;
		}

		if (n == 0) {
			break;
		}

		size_t take = (size_t)n;

		if (lines >= want) {
			take = 0;
		} else {
			// stop on the newline that finishes the line we were asked for
			for (int i = 0; i < n; i++) {
				if (buf[i] == '\n' && ++lines == want) {
					take = (size_t)(i + 1);
					break;
				}
			}
		}

		if (take > 0 && output(no_streams, buf, take) != 0) {
			return -1;
		}

		if (lines >= want) {
			// that is as many lines as were asked for
			break;
		}
	}

	return 0;
}
