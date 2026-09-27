/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/stream.h>

#define TAIL_BUFSZ 128
#define TAIL_OUTSZ 128
#define TAIL_DEFAULT 10
#define TAIL_MAXCOUNT 1000000

// tail has to keep the end of the stream around to work with it, and a ring
// of a fixed size keeps that from growing with the size of the input. the
// last lines have to fit in the ring, so a run of very long lines near the
// end can push the first of them out.
#define TAIL_RING 8192

static long want = TAIL_DEFAULT;
static bool only_out0 = false;

static char ring[TAIL_RING];
static size_t rlen = 0;

static void print_help(void) {
	printf("tail - a pcrutils utility\n\n");
	printf("tail reads in0, and copies the last lines of it out to every out stream,\n");
	printf("a line is copied out as it came in, newline and all\n");	
	printf("and the last line is copied out even with no newline on it\n\n");
	printf("usage:\n");
	printf("  tail [-n count]\n\n");
	printf("args:\n");
	printf("  -n count - copy the last count lines, 10 if not given\n");
	printf("  -o - only output to out0\n");
	printf("  -h - prints this help message\n\n");
	printf("the count can also be given the short way, as in tail -5\n");
	printf("tail reads a stream, so it takes no paths, try in a > tail\n");
	printf("the last lines have to fit in %d bytes\n", TAIL_RING);
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
		if (v > TAIL_MAXCOUNT) {
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
			printf("pcrutils: tail: unexpected argument '%s'\n", a);
			printf("tail reads in stream 0, it takes no paths\n");
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
						printf("pcrutils: tail: -n needs a count\n");
						return -1;
					}

					p = argv[++i];
				}

				if (!parse_count(p, &want)) {
					printf("pcrutils: tail: '%s' is not a count\n", p);
					return -1;
				}

				// whatever followed was the count
				break;
			}

			if (c >= '0' && c <= '9') {
				// the short way, as in tail -5
				if (!parse_count(a + j, &want)) {
					printf("pcrutils: tail: '%s' is not a count\n", a + j);
					return -1;
				}

				break;
			}

			printf("pcrutils: tail: invalid option '%c'\n", c);
			return -1;
		}
	}

	return 0;
}

static int output(int no_streams, const char* data, size_t size) {
	if (only_out0) {
		if (stream_write(0, data, size) < 0) {
			printf("pcrutils: tail: could not write to out stream 0\n");
			return -1;
		}

		return 0;
	}

	for (int o = 0; o < no_streams; o++) {
		if (stream_write(o, data, size) < 0) {
			printf("pcrutils: tail: could not write to out stream %d\n", o);
			return -1;
		}
	}

	return 0;
}

static void ring_put(const char* buf, size_t n) {
	for (size_t i = 0; i < n; i++) {
		ring[rlen % TAIL_RING] = buf[i];
		rlen++;
	}
}

// the byte at absolute spot p, the ring holds the last TAIL_RING of them
static char ring_at(size_t p) {
	return ring[p % TAIL_RING];
}

int main(int argc, char** argv) {
	int parsed = parse_args(argc, argv);

	if (parsed != 0) {
		return parsed > 0 ? 0 : -1;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: tail: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: tail: there are no out streams to write to\n");
		return -1;
	}

	int no_streams = counts[1];

	if (want == 0) {
		// nothing to copy out, but the stream still has to be read
		// so the writer is never left filling a pipe on its own
		char drain[TAIL_BUFSZ];

		for (;;) {
			int n = stream_read(0, drain, sizeof(drain));
			if (n <= 0) {
				break;
			}
		}

		return 0;
	}

	char buf[TAIL_BUFSZ];

	for (;;) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: tail: could not read in stream 0\n");
			return -1;
		}

		if (n == 0) {
			break;
		}

		ring_put(buf, (size_t)n);
	}

	// count the lines that came in, a last line with no newline of its own
	// still counts as a line
	long nlines = 0;

	for (size_t p = 0; p < rlen; p++) {
		if (ring_at(p) == '\n') {
			nlines++;
		}
	}

	if (rlen > 0 && ring_at(rlen - 1) != '\n') {
		nlines++;
	}

	// the last want lines start just past the newline that ends the line
	// before them. counting the lines first is what keeps an unterminated
	// last line from being counted one short.
	size_t begin = 0;
	long skip = nlines - want;

	if (skip > 0) {
		long seen = 0;

		for (size_t p = 0; p < rlen; p++) {
			if (ring_at(p) == '\n') {
				seen++;

				if (seen == skip) {
					begin = p + 1;
					break;
				}
			}
		}
	}

	char out[TAIL_OUTSZ];
	size_t total = rlen - begin;
	size_t off = 0;

	while (off < total) {
		size_t chunk = total - off;

		if (chunk > sizeof(out)) {
			chunk = sizeof(out);
		}

		for (size_t i = 0; i < chunk; i++) {
			out[i] = ring_at(begin + off + i);
		}

		if (output(no_streams, out, chunk) != 0) {
			return -1;
		}

		off += chunk;
	}

	return 0;
}
