/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

#define MUL_BUFSZ 128

static const char HELP[] =
	"mul - a pcrutils utility\n\n"
	"mul copies in0 to every out stream until in0 ends\n"
	"it is the way to fan a keyboard line out to several commands\n\n"
	"usage:\n"
	"  mul\n\n"
	"args:\n"
	"  -h - prints this help message\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	// mul takes no arguments at all, so anything here is wrong. the leading
	// '-' is not part of what went wrong, so start past it when it is there
	// a bare '-' is wrong too, there is nothing for mul to read from it
	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] == '-' && a[1] == '\0') {
			printf("pcrutils: mul: invalid option '-'\n");
			return -1;
		}

		printf("pcrutils: mul: invalid option '%s'\n",
		       (a[0] == '-') ? a + 1 : a);
		return -1;
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
			if (pcr_write_all(s, buf, (size_t)n) != 0) {
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
