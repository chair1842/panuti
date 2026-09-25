/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/handle.h>
#include <panuti/stream.h>
#include <panuti/errno.h>

static bool broadcast = true;

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("pcrutils: in: no file given\n");
		return -1;
	}

	if (argc > 2) {
		if (strcmp(argv[2], "-h") == 0) {
			printf("in - a pcrutils utility\n\n");
			printf("in reads all of a file's contents and outputs them to all out streams\n");
			printf("in differs from cat in that it doesn't concatenate\n\n");
			printf("usage:\n");
			printf("  in [path] [arg]\n\n");
			printf("args:\n");
			printf("  -h - prints this help message\n");
			printf("  -o - only output to out0\n");

			return 1;
		} else if (strcmp(argv[2], "-o") == 0) {
			broadcast = false;
		}
	}

	int no_streams[2] = {0};
	
	nstream(no_streams);

	for (int i = 1; i < argc; i++) {
		int fd = handle_open(argv[i]);
		if (fd < 0) {
			if (fd == PANUTIERRNO_NOTFOUND) {
				printf("pcrutils: in: %s: no such file\n", argv[i]);
			} else {
				printf("pcrutils: in: %s: could not open the file\n", argv[i]);
			}
			return -1;
		}

		char buf[128];
		for (;;) {
			int n = handle_read(fd, buf, sizeof(buf));
			if (n <= 0) {
				break;
			}

			for (int j = 0; j < n; j++) {
				if (broadcast == true) {
					for (int s = 0; s < no_streams[1]; s++) {
						stream_write(s, &buf[j], 1);
					}
				} else {
					putchar(buf[j]);
				}
			}
		}

		handle_close(fd);
	}

	return 0;
}