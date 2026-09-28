/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/stream.h>

#define CBI_BUFSZ 128

static void print_help(void) {
	printf("cbi - a pcrutils utility\n\n");
	printf("cbi takes in any amount of input streams and combines them in order.\n");
	printf("it outputs the result to out0\n\n");
	printf("args (only the first argument is considered):\n");
	printf("  -h - prints this help message\n");
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

// copy one in stream to out0 until it ends. reading a stream to the end
// before starting the next is what puts the inputs in order
static int copy_in(int in) {
	char buf[CBI_BUFSZ];

	for (;;) {
		int n = stream_read(in, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: cbi: could not read in stream %d\n", in);
			return -1;
		}

		if (n == 0) {
			return 0;
		}

		if (write_all(0, buf, (size_t)n) != 0) {
			printf("pcrutils: cbi: could not write to out0\n");
			return -1;
		}
	}
}

int main(int argc, char** argv) {
	if (argc > 1 && strcmp(argv[1], "-h") == 0) {
		print_help();
		return 1;
	}

	int counts[2] = {0, 0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: cbi: there are no in streams to read from\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: cbi: there are no out streams to write to\n");
		return -1;
	}

	int failed = 0;

	for (int s = 0; s < counts[0]; s++) {
		if (copy_in(s) != 0) {
			failed = 1;
		}
	}

	return failed ? -1 : 0;
}
