/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/handle.h>
#include <panuti/stream.h>
#include <panuti/errno.h>
#include <pcrutils/pcrutils.h>

static bool broadcast = true;

static const char HELP[] =
	"in - a pcrutils utility\n\n"
	"in reads a file's contents and outputs them to all out streams\n"
	"in differs from cat in that it doesn't concatenate, it takes one file\n\n"
	"usage:\n"
	"  in [path] [args]\n\n"
	"args go after the path\n\n"
	"args:\n"
	"  -h - prints this help message\n"
	"  -o - only output to out0\n";

static int out_file(int no_streams, const char* path) {
	int fd = handle_open(path);
	if (fd < 0) {
		if (fd == (int)PANUTIERRNO_NOTFOUND) {
			printf("pcrutils: in: %s: no such file\n", path);
		} else {
			printf("pcrutils: in: %s: could not open the file\n", path);
		}

		return -1;
	}

	int failed = 0;

	char buf[128];
	for (;;) {
		int n = handle_read(fd, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: in: %s: could not read the file\n", path);
			failed = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		if (pcr_output(no_streams, buf, (size_t)n, !broadcast) != 0) {
			printf("pcrutils: in: %s: could not write to the out streams\n", path);
			failed = 1;
			break;
		}
	}

	handle_close(fd);
	return failed ? -1 : 0;
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	if (argc < 2) {
		printf("pcrutils: in: no file given\n");
		return -1;
	}

	for (int i = 2; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: in: unexpected argument '%s'\n", a);
			return -1;
		}

		if (strcmp(a, "-o") != 0) {
			printf("pcrutils: in: invalid option '%s'\n", a + 1);
			return -1;
		}

		broadcast = false;
	}

	int counts[2] = {0};
	nstream(counts);

	int no_streams = counts[1];
	if (no_streams <= 0) {
		printf("pcrutils: in: there are no out streams to write to\n");
		return -1;
	}

	return out_file(no_streams, argv[1]);
}
