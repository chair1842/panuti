/* SPDX-License-Identifier: BSD-3-Clause */

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