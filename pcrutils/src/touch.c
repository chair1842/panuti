/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/stat.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"touch - a pcrutils utility\n\n"
	"touch makes sure a file is there, creating it when it is not\n\n"
	"usage:\n"
	"  touch <path ...>\n\n"
	"args:\n"
	"  -h - prints this help message\n\n"
	"the kernel has no timestamps yet, so there is nothing but the\n"
	"creation to do: a file that already exists is simply left alone\n\n"
	"examples:\n"
	"  touch /tmp/junk\n";

static const char* reason(int32_t rc) {
	switch (rc) {
		case PANUTIERRNO_PLAINSUCCESS: return "success";
		case PANUTIERRNO_INVALIDADDR: return "not a valid user pointer";
		case PANUTIERRNO_UNSUPPORTEDOP: return "the filesystem cannot create files";
		case -1: return "no such parent, the name is taken, or the name is too long";
		default: return NULL;
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

		printf("pcrutils: touch: invalid option '%s'\n", a + 1);
		return -1;
	}

	if (first == argc) {
		printf("pcrutils: touch: no path given\n");
		return -1;
	}

	int failed = 0;

	for (int i = first; i < argc; i++) {
		const char* path = argv[i];

		if (nexist(path)) {
			continue;
		}

		int32_t rc = panutisysf_mkfile(path);

		if (rc != PANUTIERRNO_PLAINSUCCESS) {
			const char* why = reason(rc);

			printf("pcrutils: touch: cannot create '%s': ", path);

			if (why) {
				printf("%s\n", why);
			} else {
				printf("unknown error %d\n", (int)rc);
			}

			failed = 1;
		}
	}

	return failed ? -1 : 0;
}