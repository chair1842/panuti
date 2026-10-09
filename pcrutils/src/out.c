/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/stream.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

#define OUT_BUFSZ 128

static bool append_mode = false;

static const char HELP[] =
	"out - a pcrutils utility\n\n"
	"out reads the in streams and writes them into a file. it is the\n"
	"writing half of redirection, the other half of in\n\n"
	"usage:\n"
	"  out [-a] <path>\n\n"
	"args:\n"
	"  -a - write onto the end of the file instead of wiping it\n"
	"  -h - prints this help message\n\n"
	"out is the mirror of in: in reads a file into the out streams, out\n"
	"reads the in streams into a file. try in a > out b\n"
	"the file has to already exist, there is no way to create a new one\n"
	"yet, and with no in streams wired in out has nothing to write\n";

static int write_all(int fd, const char* data, size_t len) {
	size_t off = 0;

	while (off < len) {
		int w = handle_write(fd, data + off, len - off);
		if (w <= 0) {
			return -1;
		}

		off += (size_t)w;
	}

	return 0;
}

static int out_stream(int fd, int stream) {
	char buf[OUT_BUFSZ];

	for (;;) {
		int n = stream_read(stream, buf, sizeof(buf));
		if (n < 0) {
			return -1;
		}

		if (n == 0) {
			return 0;
		}

		if (write_all(fd, buf, (size_t)n) != 0) {
			return -1;
		}
	}
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	int first = argc;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			first = i;
			break;
		}

		if (strcmp(a, "-a") != 0) {
			printf("pcrutils: out: invalid option '%s'\n", a + 1);
			return -1;
		}

		append_mode = true;
	}

	if (first == argc) {
		printf("pcrutils: out: no path given\n");
		printf("%s", HELP);
		return -1;
	}

	if (argc - first > 1) {
		printf("pcrutils: out: too many arguments\n");
		return -1;
	}

	const char* path = argv[first];

	int fd = handle_open(path);
	if (fd < 0) {
		if (fd == (int)PANUTIERRNO_NOTFOUND) {
			printf("pcrutils: out: %s: no such file\n", path);
		} else {
			printf("pcrutils: out: %s: could not open the file\n", path);
		}

		return -1;
	}

	int failed = 0;

	if (append_mode) {
		pcr_handle_to_end(fd);
	} else {
		// truncate the file to nothing before writing over it. resizing only
		// works on regular files, so a failure here just means a device that
		// does not care where it is written
		uint64_t zero = 0;
		panutisysf_resize(fd, &zero);
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: out: there are no in streams to read from\n");
		failed = 1;
		goto done;
	}

	for (int s = 0; s < counts[0]; s++) {
		if (out_stream(fd, s) != 0) {
			printf("pcrutils: out: could not write %s from the in streams\n", path);
			failed = 1;
			break;
		}
	}

done:
	handle_close(fd);
	return failed ? -1 : 0;
}