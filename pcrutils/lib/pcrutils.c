/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>

#include <panuti/stream.h>

#include <pcrutils/pcrutils.h>

int pcr_write_all(int stream, const char* buf, size_t len) {
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

int pcr_output(int no_streams, const char* buf, size_t len, int only_out0) {
	int streams = only_out0 ? 1 : no_streams;
	int failed = 0;

	for (int s = 0; s < streams; s++) {
		if (pcr_write_all(s, buf, len) != 0) {
			failed = 1;
		}
	}

	return failed ? -1 : 0;
}

// an arg is only looked at if it looks like an option, so a file that
// happens to have an h in it never brings up the help message
bool pcr_help_wanted(int argc, char** argv, const char* help) {
	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			continue;
		}

		for (int j = 1; a[j]; j++) {
			if (a[j] == 'h') {
				printf("%s", help);

				return true;
			}
		}
	}

	return false;
}