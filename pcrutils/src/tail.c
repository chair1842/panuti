/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/stream.h>

#define TAIL_BUFSZ 128
#define TAIL_OUTSZ 128
#define TAIL_DEFAULT 10
#define TAIL_MAXCOUNT 1000000

static long want = TAIL_DEFAULT;
static bool only_out0 = false;

// tail has to keep the end of the stream around to work with it. the window
// below holds the last want lines and grows with realloc as they come in, so
// a run of very long lines near the end no longer pushes the first of them
// out, and the memory taken is only what the lines it keeps weigh.
// win_head marks where the live bytes start, once a trim has thrown some of
// the front away, and win_lines counts the newlines still live.
static char* win;
static size_t win_cap;
static size_t win_head;
static size_t win_len;
static long win_lines;

// the line being built up, which only joins the window when its newline
// turns up, and part_len is 0 between lines
static char* part;
static size_t part_cap;
static size_t part_len;

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
	printf("the lines it keeps have to fit in what is left of memory\n");
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

// the only out stream, or all of them
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

// streams can take less than we give them, so keep writing until the window
// is out or the stream stops making progress
static int write_all(int no_streams, const char* data, size_t size) {
	size_t off = 0;

	while (off < size) {
		size_t chunk = size - off;

		if (chunk > TAIL_OUTSZ) {
			chunk = TAIL_OUTSZ;
		}

		if (output(no_streams, data + off, chunk) != 0) {
			return -1;
		}

		off += chunk;
	}

	return 0;
}

// grow the window so it can take `need` bytes in all
static int win_reserve(size_t need) {
	if (need <= win_cap) {
		return 0;
	}

	size_t cap = win_cap ? win_cap : TAIL_BUFSZ;

	while (cap < need) {
		if (cap > (size_t)-1 / 2) {
			return -1;
		}
		cap *= 2;
	}

	char* p = realloc(win, cap);
	if (!p) {
		return -1;
	}

	win = p;
	win_cap = cap;
	return 0;
}

// slide the live bytes back to the front, once the thrown away ones at the
// front have grown to be a burden on their own
static void win_compact(void) {
	if (win_head == 0) {
		return;
	}

	size_t live = win_len - win_head;

	memmove(win, win + win_head, live);
	win_head = 0;
	win_len = live;
}

static int win_append(const char* src, size_t n) {
	// make room by tidying up first, so a window that keeps sliding along
	// does not creep forward one line at a time
	if (win_head > 0 && win_len + n > win_cap) {
		win_compact();
	}

	if (win_reserve(win_len + n) != 0) {
		return -1;
	}

	memcpy(win + win_len, src, n);

	for (size_t i = 0; i < n; i++) {
		if (src[i] == '\n') {
			win_lines++;
		}
	}

	win_len += n;
	return 0;
}

// drop whole lines off the front until at most `target` are left
static void win_trim_to(long target) {
	if (win_lines <= target) {
		return;
	}

	long drop = win_lines - target;
	size_t p = win_head;

	while (drop > 0 && p < win_len) {
		if (win[p] == '\n') {
			drop--;
			win_lines--;
		}
		p++;
	}

	win_head = p;

	if (win_head > 0 && win_head * 2 >= win_len) {
		win_compact();
	}
}

// a byte onto the line being built, and its newline moves that line into the
// window and lets the window throw away anything past the last want lines
static int part_push(char c) {
	if (part_len + 1 > part_cap) {
		size_t cap = part_cap ? part_cap * 2 : TAIL_BUFSZ;

		if (cap < part_len + 1) {
			cap = part_len + 1;
		}

		char* p = realloc(part, cap);
		if (!p) {
			return -1;
		}

		part = p;
		part_cap = cap;
	}

	part[part_len++] = c;

	if (c != '\n') {
		return 0;
	}

	if (win_append(part, part_len) != 0) {
		return -1;
	}

	part_len = 0;
	win_trim_to(want);
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
		printf("pcrutils: tail: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: tail: there are no out streams to write to\n");
		return -1;
	}

	int no_streams = counts[1];

	win_head = 0;
	win_len = 0;
	win_lines = 0;
	part_len = 0;

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

	int rc = 0;
	char buf[TAIL_BUFSZ];

	for (;;) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: tail: could not read in stream 0\n");
			rc = -1;
			break;
		}

		if (n == 0) {
			break;
		}

		for (int i = 0; i < n; i++) {
			if (part_push(buf[i]) != 0) {
				printf("pcrutils: tail: out of memory\n");
				rc = -1;
				break;
			}
		}

		if (rc != 0) {
			break;
		}
	}

	if (rc == 0) {
		// a last line with no newline of its own is a line too, so it takes
		// one of the count and the window gives up a line to make room
		win_trim_to(part_len > 0 ? want - 1 : want);

		if (win_len > win_head &&
		    write_all(no_streams, win + win_head, win_len - win_head) != 0) {
			rc = -1;
		}
	}

	if (rc == 0 && part_len > 0) {
		if (write_all(no_streams, part, part_len) != 0) {
			rc = -1;
		}
	}

	free(win);
	free(part);
	win = NULL;
	part = NULL;
	win_cap = 0;
	part_cap = 0;
	part_len = 0;

	return rc;
}
