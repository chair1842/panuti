/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"cksum - a pcrutils utility\n\n"
	"cksum walks every byte that runs into it and prints the crc, and\n"
	"the length in bytes, of what it saw\n\n"
	"usage:\n"
	"  cksum\n\n"
	"args:\n"
	"  -h - prints this help message\n\n"
	"the result is one line on out0. the crc is the posix one, so the\n"
	"same input always makes the same number\n\n"
	"cksum reads a stream, so it takes no paths, try in a > cksum\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	if (argc > 1) {
		const char* a = argv[1];
		printf("pcrutils: cksum: unexpected argument '%s'\n", a);
		printf("cksum reads in stream 0, it takes no paths\n");
		return -1;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: cksum: there is no in stream to read\n");
		return -1;
	}

	unsigned long crc = 0;
	unsigned long len = 0;
	int failed = 0;

	char buf[128];

	for (;;) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: cksum: could not read the in stream\n");
			failed = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		for (int i = 0; i < n; i++) {
			crc ^= ((unsigned long)(unsigned char)buf[i]) << 24;

			for (int b = 0; b < 8; b++) {
				crc = (crc << 1) ^ ((crc & 0x80000000UL) ? 0x04C11DB7UL : 0);
			}
		}

		len += (unsigned long)n;
	}

	if (!failed) {
		printf("%lu %lu\n", crc, len);
	}

	return failed ? -1 : 0;
}