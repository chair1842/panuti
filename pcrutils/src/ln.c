/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"ln - a pcrutils utility\n\n"
	"ln gives a node a second name, a hard link to it\n\n"
	"usage:\n"
	"  ln <target> <link>\n\n"
	"args:\n"
	"  -h - prints this help message\n\n"
	"a hard link is a second name for one inode, so both names have to sit on the same filesystem.\n\n"
	"examples:\n"
	"  ln /tmp/a /tmp/b\n";

static const char* reason(int32_t rc) {
	switch (rc) {
		case PANUTIERRNO_PLAINSUCCESS: return "success";
		case PANUTIERRNO_INVALIDADDR: return "not a valid user pointer";
		case PANUTIERRNO_NOTFOUND: return "no such file or directory";
		case PANUTIERRNO_EXISTS: return "the link name is already there";
		case PANUTIERRNO_UNSUPPORTEDOP: return "the two names are not on the same filesystem";
		case PANUTIERRNO_INVALIDARG: return "bad argument";
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

		printf("pcrutils: ln: invalid option '%s'\n", a + 1);
		return -1;
	}

	if (first == argc) {
		printf("pcrutils: ln: a target and a link name are needed\n");
		printf("%s", HELP);
		return -1;
	}

	if (argc - first < 2) {
		printf("pcrutils: ln: a link name is needed\n");
		return -1;
	}

	if (argc - first > 2) {
		printf("pcrutils: ln: too many arguments\n");
		return -1;
	}

	const char* target = argv[first];
	const char* link = argv[first + 1];

	int32_t rc = panutisysf_link(target, link);

	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		const char* why = reason(rc);

		printf("pcrutils: ln: cannot link '%s' as '%s': ", target, link);

		if (why) {
			printf("%s\n", why);
		} else {
			printf("unknown error %d\n", (int)rc);
		}

		return -1;
	}

	return 0;
}