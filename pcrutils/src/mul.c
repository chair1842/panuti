/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>

#define MUL_BUFSZ 128

static void print_help(void) {
	printf("mul - a pcrutils utility\n\n");
	printf("mul copies in stream 0 to every out stream until in stream 0 ends\n");
	printf("it is the way to fan a keyboard line out to several commands\n\n");
	printf("usage:\n");
	printf("  mul\n\n");
	printf("args:\n");
	printf("  -h - prints this help message\n");
}

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

int main(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		for (int j = 0; a[j]; j++) {
			if (a[j] == 'h') {
				print_help();
				return 1;
			}

			printf("pcrutils: mul: invalid option '%c'\n", a[j]);
			return -1;
		}
	}

	int counts[2] = {0, 0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: mul: there are no in streams to read from\n");
		return -1;
	}

	int no_streams = counts[1];
	if (no_streams < 1) {
		printf("pcrutils: mul: there are no out streams to write to\n");
		return -1;
	}

	int failed = 0;
	char buf[MUL_BUFSZ];

	for (;;) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: mul: could not read the in stream\n");
			failed = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		for (int s = 0; s < no_streams; s++) {
			if (write_all(s, buf, (size_t)n) != 0) {
				failed = 1;
			}
		}

		if (failed) {
			printf("pcrutils: mul: could not write to the out streams\n");
			break;
		}
	}

	return failed ? -1 : 0;
}
