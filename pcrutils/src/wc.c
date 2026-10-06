/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

#define WC_BUFSZ 128
#define WC_LABELSZ 16

static bool want_lines = false;
static bool want_words = false;
static bool want_chars = false;
static bool want_streams = false;

typedef struct {
	size_t lines;
	size_t words;
	size_t chars;
	bool in_word;
	char last;
} counter_t;

static const char HELP[] =
	"wc - a pcrutils utility\n\n"
	"wc counts the lines, words and characters of its in streams and of\n"
	"any files it is given. an in stream is called in0, in1 and so on,\n"
	"a file is called by the path given\n\n"
	"given a file, wc counts that file and leaves the in streams alone,\n"
	"and -s asks it to count the in streams as well. without a file it\n"
	"counts the in streams either way\n\n"
	"the count goes to out0, the same place the errors go\n\n"
	"usage:\n"
	"  wc [-l] [-w] [-c] [-s] [path ...]\n\n"
	"args:\n"
	"  -l - count lines\n"
	"  -w - count words\n"
	"  -c - count characters\n"
	"  -s - count the in streams as well as the files\n"
	"  -h - prints this help message\n\n"
	"with no counts given, all three are printed\n"
	"with -s and nothing piped in, wc waits on in0 the way cat does\n";

static void count_chunk(counter_t* c, const char* buf, size_t len) {
	c->chars += len;
	c->last = buf[len - 1];

	for (size_t i = 0; i < len; i++) {
		char ch = buf[i];

		if (ch == '\n') {
			c->lines++;
		}

		// a word is a run of anything that is not a space, the same six
		// that isspace counts
		if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f') {
			c->in_word = false;
		} else if (!c->in_word) {
			c->in_word = true;
			c->words++;
		}
	}
}

// a last line with no newline of its own is still a line
static void count_finish(counter_t* c) {
	if (c->chars > 0 && c->last != '\n') {
		c->lines++;
	}
}

static int count_stream(int s, counter_t* c) {
	char buf[WC_BUFSZ];

	for (;;) {
		int n = stream_read(s, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: wc: could not read in stream %d\n", s);
			return -1;
		}

		if (n == 0) {
			break;
		}

		count_chunk(c, buf, (size_t)n);
	}

	count_finish(c);
	return 0;
}

static int count_file(const char* path, counter_t* c) {
	int fd = handle_open(path);
	if (fd < 0) {
		if (fd == (int)PANUTIERRNO_NOTFOUND) {
			printf("pcrutils: wc: %s: no such file\n", path);
		} else {
			printf("pcrutils: wc: %s: could not open the file\n", path);
		}

		return -1;
	}

	char buf[WC_BUFSZ];
	int failed = 0;

	for (;;) {
		int n = handle_read(fd, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: wc: %s: could not read the file\n", path);
			failed = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		count_chunk(c, buf, (size_t)n);
	}

	handle_close(fd);

	if (!failed) {
		count_finish(c);
	}

	return failed ? -1 : 0;
}

// there is no sprintf here, so the "inN" names get built by hand
static void stream_label(int s, char* out, size_t cap) {
	char digits[12];
	int len = 0;
	int n = s;

	if (n == 0) {
		digits[len++] = '0';
	}

	while (n > 0 && len < (int)sizeof(digits)) {
		digits[len++] = (char)('0' + (n % 10));
		n /= 10;
	}

	size_t used = 0;

	if (cap > 2) {
		out[used++] = 'i';
		out[used++] = 'n';
	}

	for (int i = len - 1; i >= 0 && used + 1 < cap; i--) {
		out[used++] = digits[i];
	}

	out[used] = '\0';
}

static void report(const counter_t* c, const char* label) {
	int printed = 0;

	if (want_lines) {
		printf("%zu", c->lines);
		printed = 1;
	}

	if (want_words) {
		printf(printed ? " %zu" : "%zu", c->words);
		printed = 1;
	}

	if (want_chars) {
		printf(printed ? " %zu" : "%zu", c->chars);
	}

	printf(" %s\n", label);
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

		if (strcmp(a, "-l") == 0) {
			want_lines = true;
		} else if (strcmp(a, "-w") == 0) {
			want_words = true;
		} else if (strcmp(a, "-c") == 0) {
			want_chars = true;
		} else if (strcmp(a, "-s") == 0) {
			want_streams = true;
		} else {
			printf("pcrutils: wc: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	// no counts asked for means all of them, like wc does
	if (!want_lines && !want_words && !want_chars) {
		want_lines = true;
		want_words = true;
		want_chars = true;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1 && first == argc) {
		printf("pcrutils: wc: there is nothing to count\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: wc: there are no out streams to write to\n");
		return -1;
	}

	int failed = 0;

	// a named file is counted on its own unless -s asks for the in streams
	// too, and with no file to work on the in streams are counted either
	// way. reading in0 with no pipe on it would just sit waiting on the
	// keyboard, the same as cat does.
	if (first == argc || want_streams) {
		for (int s = 0; s < counts[0]; s++) {
			counter_t c = {0, 0, 0, false, '\0'};
			char label[WC_LABELSZ];

			stream_label(s, label, sizeof(label));

			if (count_stream(s, &c) != 0) {
				failed = 1;
				continue;
			}

			report(&c, label);
		}
	}

	for (int i = first; i < argc; i++) {
		counter_t c = {0, 0, 0, false, '\0'};

		if (count_file(argv[i], &c) != 0) {
			failed = 1;
			continue;
		}

		report(&c, argv[i]);
	}

	return failed ? -1 : 0;
}
