/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/stream.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

#define TEE_BUFSZ 128
#define TEE_MAXFILES 8

static bool append_mode = false;

static const char HELP[] =
	"tee - a pcrutils utility\n\n"
	"tee reads in0 and writes it into every file it is given. it does not\n"
	"touch the out streams; broadcasting to files is its only thing, the\n"
	"out that reaches more than one place at once\n\n"
	"usage:\n"
	"  tee [-a] <path ...>\n\n"
	"args:\n"
	"  -a - write onto the end of each file instead of wiping it\n"
	"  -h - prints this help message\n\n"
	"files that are not there yet are created on the spot\n\n"
	"examples:\n"
	"  in a > tee b c\n"
	"  in a > tee -a log\n";

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
			printf("pcrutils: tee: invalid option '%s'\n", a + 1);
			return -1;
		}

		append_mode = true;
	}

	if (first == argc) {
		printf("pcrutils: tee: no file given to write to\n");
		printf("%s", HELP);
		return -1;
	}

	int nfiles = argc - first;
	if (nfiles > TEE_MAXFILES) {
		printf("pcrutils: tee: no more than %d files at once\n", TEE_MAXFILES);
		return -1;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: tee: there is no in stream to read\n");
		return -1;
	}

	int fds[TEE_MAXFILES];
	int opened = 0;
	int failed = 0;

	for (int i = first; i < argc; i++) {
		int fd = pcr_open_or_create(argv[i]);
		if (fd < 0) {
			printf("pcrutils: tee: %s: %s\n", argv[i],
			       fd == (int)PANUTIERRNO_NOTFOUND ? "no such path" : "could not open it");
			failed = 1;
			break;
		}

		if (append_mode) {
			pcr_handle_to_end(fd);
		} else {
			uint64_t zero = 0;
			panutisysf_resize(fd, &zero);
		}

		fds[opened++] = fd;
	}

	char buf[TEE_BUFSZ];

	while (!failed) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: tee: could not read the in stream\n");
			failed = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		for (int i = 0; i < opened; i++) {
			if (pcr_handle_write_all(fds[i], buf, (size_t)n) != 0) {
				printf("pcrutils: tee: could not write to a file\n");
				failed = 1;
				break;
			}
		}
	}

	for (int i = 0; i < opened; i++) {
		handle_close(fds[i]);
	}

	return failed ? -1 : 0;
}