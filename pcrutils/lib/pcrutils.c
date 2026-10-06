/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

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

bool pcr_help_wanted(int argc, char** argv, const char* help) {
	if (!pcr_option_provided(argc, argv, "h")) {
		return false;
	}

	printf("%s", help);

	return true;
}

bool pcr_option_provided(int argc, char** argv, const char* option) {
	const char* opt = (option[0] == '-') ? option + 1 : option;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			continue;
		}

		if (strcmp(a + 1, opt) == 0) {
			return true;
		}
	}

	return false;
}