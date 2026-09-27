/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/stream.h>

#define SPLIT_BUFSZ 128
#define SPLIT_OUTSZ 128
// the input has to be held whole before it can be cut into equal parts, and
// there is no malloc here, so the size is fixed. the extra byte is what lets
// the read loop poke past the end to see whether the input is bigger.
#define SPLIT_MAX 65536

#define SPLIT_LINES 0
#define SPLIT_WORDS 1
#define SPLIT_CHARS 2

static char buf[SPLIT_MAX + 1];

static int mode = SPLIT_LINES;

static void print_help(void) {
	printf("split - a pcrutils utility\n\n");
	printf("split reads in stream 0, cuts it into as many parts as there are\n");
	printf("out streams, and gives one part to each out stream. the parts are\n");
	printf("cut as evenly as they can be, so they come out the same size to\n");
	printf("within one line, word or character, and putting them back together\n");
	printf("gives the input back\n\n");
	printf("with fewer lines, words or characters than there are out streams,\n");
	printf("some of the parts come out empty\n\n");
	printf("the whole input is held in memory, and it has to fit in %d bytes\n\n", SPLIT_MAX);
	printf("usage:\n");
	printf("  split [-w] [-c]\n\n");
	printf("args:\n");
	printf("  -w - cut on words, the default is to cut on lines\n");
	printf("  -c - cut on characters\n");
	printf("  -h - prints this help message\n\n");
	printf("split reads a stream, so it takes no paths, try in a > split\n");
}

// a word is a run of anything that is not a space, the same six that
// isspace counts
static bool is_space(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

// the spot just past the part that starts at p, which is where the next part
// begins
static size_t next_boundary(size_t p, size_t len) {
	if (p >= len) {
		return len;
	}

	if (mode == SPLIT_CHARS) {
		return p + 1;
	}

	if (mode == SPLIT_WORDS) {
		// the word, and then the space that follows it, so that the parts
		// put back together give the input back exactly
		while (p < len && !is_space(buf[p])) {
			p++;
		}

		while (p < len && is_space(buf[p])) {
			p++;
		}

		return p;
	}

	// a line takes its newline with it, and a last line with no newline of
	// its own still gets a part to itself
	while (p < len && buf[p] != '\n') {
		p++;
	}

	if (p < len) {
		p++;
	}

	return p;
}

static size_t count_parts(size_t len) {
	size_t n = 0;
	size_t p = 0;

	while (p < len) {
		p = next_boundary(p, len);
		n++;
	}

	return n;
}

// the spot just past the k'th part, k running from 0 to nparts
static size_t boundary(size_t k, size_t len) {
	size_t p = 0;

	for (size_t i = 0; i < k; i++) {
		p = next_boundary(p, len);
	}

	return p;
}

// returns 0 to carry on, 1 when the help was asked for, -1 on a bad arg
static int parse_args(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: split: unexpected argument '%s'\n", a);
			printf("split reads in stream 0, it takes no paths\n");
			return -1;
		}

		for (int j = 1; a[j]; j++) {
			char c = a[j];

			if (c == 'h') {
				print_help();
				return 1;
			}

			if (c == 'w') {
				mode = SPLIT_WORDS;
				continue;
			}

			if (c == 'c') {
				mode = SPLIT_CHARS;
				continue;
			}

			printf("pcrutils: split: invalid option '%c'\n", c);
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
		printf("pcrutils: split: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: split: there are no out streams to write to\n");
		return -1;
	}

	int no_streams = counts[1];

	// the input is read whole, because a fair cut needs to know how much
	// of it there is before it can work out where the cuts go
	size_t len = 0;

	for (;;) {
		int n = stream_read(0, buf + len, SPLIT_MAX - len);
		if (n < 0) {
			printf("pcrutils: split: could not read in stream 0\n");
			return -1;
		}

		if (n == 0) {
			break;
		}

		len += (size_t)n;

		if (len == SPLIT_MAX) {
			// look for one more byte to know the input did not stop here
			int m = stream_read(0, buf + len, 1);

			if (m > 0) {
				printf("pcrutils: split: the input is bigger than %d bytes\n", SPLIT_MAX);
				return -1;
			}
		}
	}

	size_t nparts = count_parts(len);

	if (nparts == 0) {
		// nothing came in, so there is nothing to hand out
		return 0;
	}

	// each part runs up to its share of the parts there are, so the parts
	// come out the same size to within one unit
	for (int s = 0; s < no_streams; s++) {
		size_t from_units = (size_t)(((long)s * (long)nparts) / no_streams);
		size_t to_units = (size_t)((((long)s + 1) * (long)nparts) / no_streams);

		size_t from = boundary(from_units, len);
		size_t to = boundary(to_units, len);
		size_t off = from;

		while (off < to) {
			size_t chunk = to - off;

			if (chunk > SPLIT_OUTSZ) {
				chunk = SPLIT_OUTSZ;
			}

			if (stream_write(s, buf + off, chunk) < 0) {
				printf("pcrutils: split: could not write to out stream %d\n", s);
				return -1;
			}

			off += chunk;
		}
	}

	return 0;
}
