/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

#define CAT_BUFSZ 128

static bool only_out0 = false;

static void print_help(void) {
	printf("cat - a pcrutils utility\n\n");
	printf("cat copies files to all out streams, or copies in0\n");
	printf("when no file is given\n\n");
	printf("usage:\n");
	printf("  cat [path ...]\n\n");
	printf("args:\n");
	printf("  -o - only output to out0\n");
	printf("  -h - prints this help message\n");
}

static int copy_in(int no_streams) {
	int counts[2] = {0, 0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: cat: there are no in streams to read from\n");
		return -1;
	}

	char buf[CAT_BUFSZ];
	int failed = 0;

	for (;;) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: cat: could not read the in stream\n");
			failed = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		if (pcr_output(no_streams, buf, (size_t)n, only_out0) != 0) {
			printf("pcrutils: cat: could not write to the out streams\n");
			failed = 1;
			break;
		}
	}

	return failed ? -1 : 0;
}

static int copy_file(int no_streams, const char* path) {
	int fd = handle_open(path);
	if (fd < 0) {
		if (fd == (int)PANUTIERRNO_NOTFOUND) {
			printf("pcrutils: cat: %s: no such file\n", path);
		} else {
			printf("pcrutils: cat: %s: could not open the file\n", path);
		}

		return -1;
	}

	int failed = 0;
	char buf[CAT_BUFSZ];

	for (;;) {
		int n = handle_read(fd, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: cat: %s: could not read the file\n", path);
			failed = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		if (pcr_output(no_streams, buf, (size_t)n, only_out0) != 0) {
			printf("pcrutils: cat: %s: could not write to the out streams\n", path);
			failed = 1;
			break;
		}
	}

	handle_close(fd);
	return failed ? -1 : 0;
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
			if (a[j] == 'o') {
				only_out0 = true;
			} else if (a[j] == 'h') {
				print_help();
				return 1;
			} else {
				printf("pcrutils: cat: invalid option '%c'\n", a[j]);
				return -1;
			}
		}
	}

	int counts[2] = {0, 0};
	nstream(counts);

	int no_streams = counts[1];
	if (no_streams < 1) {
		printf("pcrutils: cat: there are no out streams to write to\n");
		return -1;
	}

	if (first == argc) {
		return copy_in(no_streams);
	}

	int failed = 0;

	for (int i = first; i < argc; i++) {
		if (copy_file(no_streams, argv[i]) != 0) {
			failed = 1;
		}
	}

	return failed ? -1 : 0;
}
