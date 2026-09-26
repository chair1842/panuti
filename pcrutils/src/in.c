/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/handle.h>
#include <panuti/stream.h>
#include <panuti/errno.h>

static bool broadcast = true;

static void print_help(void) {
	printf("in - a pcrutils utility\n\n");
	printf("in reads a file's contents and outputs them to all out streams\n");
	printf("in differs from cat in that it doesn't concatenate, it takes one file\n\n");
	printf("usage:\n");
	printf("  in [path] [args]\n\n");
	printf("args go after the path\n\n");
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
	int streams = broadcast ? no_streams : 1;
	int failed = 0;

	for (int s = 0; s < streams; s++) {
		if (write_all(s, buf, len) != 0) {
			failed = 1;
		}
	}

	return failed ? -1 : 0;
}

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

		if (output(no_streams, buf, (size_t)n) != 0) {
			printf("pcrutils: in: %s: could not write to the out streams\n", path);
			failed = 1;
			break;
		}
	}

	handle_close(fd);
	return failed ? -1 : 0;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("pcrutils: in: no file given\n");
		return -1;
	}

	for (int i = 2; i < argc; i++) {
		const char* a = argv[i];

		for (int j = 1; a[j]; j++) {
			if (a[j] == 'o') {
				broadcast = false;
			} else if (a[j] == 'h') {
				print_help();
				return 1;
			} else {
				printf("pcrutils: in: invalid option '%c'\n", a[j]);
				return -1;
			}
		}
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
