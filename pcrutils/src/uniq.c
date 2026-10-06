/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

#define UNIQ_NUMSZ 16

static bool show_counts = false;
static bool only_dups = false;
static bool only_unique = false;
static bool only_out0 = false;

static const char HELP[] =
	"uniq - a pcrutils utility\n\n"
	"uniq reads in0, and drops lines that repeat the line above them\n"
	"it writes the result out to every out stream\n\n"
	"usage:\n"
	"  uniq [-c] [-d] [-u] [-o]\n\n"
	"args:\n"
	"  -c - print how many times each line showed up before it\n"
	"  -d - keep a line only when it repeated\n"
	"  -u - keep a line only when it did not repeat\n"
	"  -o - only output to out0\n"
	"  -h - prints this help message\n\n"
	"uniq reads a stream, so it takes no paths, try in a > uniq\n";

// the counts are not a stream payload, they go in front of the line
static int emit_counted(int no_streams, int only_out0, size_t count, const char* line) {
	char head[UNIQ_NUMSZ];
	int hn = pcr_uformat((unsigned long)count, 10, head);

	if (hn <= 0) {
		return -1;
	}

	if (only_out0) {
		stream_write(0, head, (size_t)hn);
		stream_write(0, " ", 1);
		return stream_write(0, line, strlen(line)) < 0 ? -1 : 0;
	}

	for (int s = 0; s < no_streams; s++) {
		if (pcr_write_all(s, head, (size_t)hn) != 0 ||
		    pcr_write_all(s, " ", 1) != 0 ||
		    pcr_write_all(s, line, strlen(line)) != 0) {
			return -1;
		}
	}

	return 0;
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: uniq: unexpected argument '%s'\n", a);
			printf("uniq reads in stream 0, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-c") == 0) {
			show_counts = true;
		} else if (strcmp(a, "-d") == 0) {
			only_dups = true;
		} else if (strcmp(a, "-u") == 0) {
			only_unique = true;
		} else if (strcmp(a, "-o") == 0) {
			only_out0 = true;
		} else {
			printf("pcrutils: uniq: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: uniq: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: uniq: there are no out streams to write to\n");
		return -1;
	}

	pcr_lines_t lines;
	if (pcr_stream_lines(0, &lines) != 0) {
		printf("pcrutils: uniq: could not read the in stream\n");
		return -1;
	}

	int failed = 0;
	size_t run_start = 0;

	for (size_t i = 1; i <= lines.n; i++) {
		bool same = (i < lines.n) && strcmp(lines.v[i], lines.v[run_start]) == 0;

		if (same) {
			continue;
		}

		size_t run_len = i - run_start;

		if (show_counts) {
			if (emit_counted(counts[1], only_out0, run_len, lines.v[run_start]) != 0) {
				failed = 1;
			}
		} else if ((only_dups && run_len > 1) ||
		           (only_unique && run_len == 1) ||
		           (!only_dups && !only_unique)) {
			if (pcr_output(counts[1], lines.v[run_start], strlen(lines.v[run_start]), only_out0) != 0) {
				failed = 1;
			}
		}

		if (failed) {
			break;
		}

		run_start = i;
	}

	pcr_lines_free(&lines);

	if (failed) {
		printf("pcrutils: uniq: could not write to the out streams\n");
		return -1;
	}

	return 0;
}