/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"truncate - a pcrutils utility\n\n"
	"truncate sets a file's size, shrinking it or letting it stretch to\n"
	"empty space\n\n"
	"usage:\n"
	"  truncate [-s <size>] <path ...>\n\n"
	"args:\n"
	"  -s <size> - the size to set the file to, in bytes, default 0\n"
	"  -h - prints this help message\n\n"
	"the current directory or a corner of the console is no place for it,\n"
	"only regular files take a resize\n\n"
	"examples:\n"
	"  truncate /tmp/junk\n"
	"  truncate -s 1024 /tmp/big\n";

static const char* reason(int32_t rc) {
	switch (rc) {
		case PANUTIERRNO_PLAINSUCCESS: return "success";
		case PANUTIERRNO_BADFD: return "the file cannot be resized (is it a device?)";
		case PANUTIERRNO_UNSUPPORTEDOP: return "the filesystem does not allow resizing";
		case PANUTIERRNO_NOMEM: return "out of memory";
		default: return NULL;
	}
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	long size = 0;
	bool have_size = false;
	int first = argc;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			first = i;
			break;
		}

		if (strcmp(a, "-s") == 0) {
			if (i + 1 >= argc) {
				printf("pcrutils: truncate: -s needs a size\n");
				return -1;
			}

			bool ok;
			size = pcr_parse_long(argv[++i], &ok);
			if (!ok) {
				printf("pcrutils: truncate: '%s' is not a size\n", argv[i]);
				return -1;
			}

			have_size = true;
		} else {
			printf("pcrutils: truncate: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	if (first == argc) {
		printf("pcrutils: truncate: no path given\n");
		return -1;
	}

	uint64_t newly = have_size ? (uint64_t)size : 0;
	int failed = 0;

	for (int i = first; i < argc; i++) {
		const char* path = argv[i];

		int fd = handle_open(path);
		if (fd < 0) {
			printf("pcrutils: truncate: %s: %s\n", path,
			       fd == (int)PANUTIERRNO_NOTFOUND ? "no such file" : "could not open it");
			failed = 1;
			continue;
		}

		uint64_t set = newly;
		int32_t rc = panutisysf_resize(fd, &set);

		handle_close(fd);

		if (rc != PANUTIERRNO_PLAINSUCCESS) {
			const char* why = reason(rc);

			printf("pcrutils: truncate: %s: ", path);

			if (why) {
				printf("%s\n", why);
			} else {
				printf("cannot set the size, error %d\n", (int)rc);
			}

			failed = 1;
		}
	}

	return failed ? -1 : 0;
}