/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

#define SPLIT_BUFSZ 128
#define SPLIT_OUTSZ 128
// the input has to be held whole before it can be cut into equal parts, so the
// buffer is grown with realloc as the stream comes in and the size of the
// input is whatever the machine can fit
#define SPLIT_INITIAL 4096

#define SPLIT_LINES 0
#define SPLIT_WORDS 1
#define SPLIT_CHARS 2

static char* buf;
static size_t buf_cap;

static int mode = SPLIT_LINES;

static const char HELP[] =
	"split - a pcrutils utility\n\n"
	"split reads in0, cuts it into as many parts as there are out streams,\n"
	"and gives one part to each out stream.\n"
	"the parts are cut as evenly as they can be, so they come out the same size to within one line,\n"
	"word or character, and putting them back together gives the input back\n\n"
	"with fewer lines, words or characters than there are out streams,\n"
	"some of the parts come out empty\n\n"
	"the whole input is held in memory,\n"
	"so it has to fit in what is left of memory once everything else has taken its share\n\n"
	"usage:\n"
	"  split [-w] [-c]\n\n"
	"args:\n"
	"  -w - cut on words, the default is to cut on lines\n"
	"  -c - cut on characters\n"
	"  -h - prints this help message\n\n"
	"split reads a stream, so it takes no paths, try in a > split\n";

// grow the input buffer so it can take `need` bytes in all
static int buf_reserve(size_t need) {
	if (need <= buf_cap) {
		return 0;
	}

	size_t want = buf_cap ? buf_cap : SPLIT_INITIAL;

	while (want < need) {
		if (want > (size_t)-1 / 2) {
			return -1;
		}
		
		want *= 2;
	}

	char* p = realloc(buf, want);
	if (!p) {
		return -1;
	}

	buf = p;
	buf_cap = want;
	return 0;
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

// returns 0 to carry on, -1 on a bad arg
static int parse_args(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: split: unexpected argument '%s'\n", a);
			printf("split reads in stream 0, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-w") == 0) {
			mode = SPLIT_WORDS;
		} else if (strcmp(a, "-c") == 0) {
			mode = SPLIT_CHARS;
		} else {
			printf("pcrutils: split: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	return 0;
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	int parsed = parse_args(argc, argv);

	if (parsed != 0) {
		return -1;
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

	int rc = 0;

	// the input is read whole, because a fair cut needs to know how much
	// of it there is before it can work out where the cuts go
	size_t len = 0;

	for (;;) {
		if (buf_reserve(len + 2) != 0) {
			printf("pcrutils: split: out of memory\n");
			rc = -1;
			break;
		}

		int n = stream_read(0, buf + len, buf_cap - len);
		if (n < 0) {
			printf("pcrutils: split: could not read in stream 0\n");
			rc = -1;
			break;
		}

		if (n == 0) {
			break;
		}

		len += (size_t)n;
	}

	if (rc == 0) {
		size_t nparts = count_parts(len);

		for (int s = 0; s < no_streams && nparts > 0; s++) {
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
					rc = -1;
					break;
				}

				off += chunk;
			}

			if (rc != 0) {
				break;
			}
		}
	}

	free(buf);
	buf = NULL;
	buf_cap = 0;

	return rc;
}
