/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <panuti/errno.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"mv - a pcrutils utility\n\n"
	"mv changes a node's name, moving it to another path\n\n"
	"usage:\n"
	"  mv <from> <to>\n\n"
	"args:\n"
	"  -h - prints this help message\n\n"
	"mv works on one filesystem at a time."
	"renaming across a mount is not a thing the kernel can do.\n"
	"moving inside a mounted filesystem is what the filesystem allows\n\n"
	"examples:\n"
	"  mv /tmp/a /tmp/b\n";

static const char* reason(int32_t rc) {
	switch (rc) {
		case PANUTIERRNO_PLAINSUCCESS: return "success";
		case PANUTIERRNO_INVALIDADDR: return "not a valid user pointer";
		case PANUTIERRNO_NOTFOUND: return "no such file or directory";
		case PANUTIERRNO_EXISTS: return "the destination is already there";
		case PANUTIERRNO_UNSUPPORTEDOP: return "the filesystem does not allow it";
		case PANUTIERRNO_INVALIDARG: return "bad argument";
		case PANUTIERRNO_NOMEM: return "out of memory";
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

		printf("pcrutils: mv: invalid option '%s'\n", a + 1);
		return -1;
	}

	if (first == argc) {
		printf("pcrutils: mv: a from and a to are needed\n");
		printf("%s", HELP);
		return -1;
	}

	if (argc - first < 2) {
		printf("pcrutils: mv: a to is needed\n");
		return -1;
	}

	if (argc - first > 2) {
		printf("pcrutils: mv: too many arguments\n");
		return -1;
	}

	const char* from = argv[first];
	const char* to = argv[first + 1];

	int32_t rc = panutisysf_rename(from, to);

	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		const char* why = reason(rc);

		printf("pcrutils: mv: cannot move '%s' to '%s': ", from, to);

		if (why) {
			printf("%s\n", why);
		} else {
			printf("unknown error %d\n", (int)rc);
		}

		return -1;
	}

	return 0;
}