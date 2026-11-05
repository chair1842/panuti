/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/stat.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"touch - a pcrutils utility\n\n"
	"touch creates missing files and moves an existing file's access and\n"
	"modification times to now\n\n"
	"usage:\n"
	"  touch <path ...>\n\n"
	"args:\n"
	"  -h - prints this help message\n\n"
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

static const char* set_reason(int32_t rc) {
	switch (rc) {
		case PANUTIERRNO_PLAINSUCCESS: return "success";
		case PANUTIERRNO_INVALIDADDR: return "not a valid user pointer";
		case PANUTIERRNO_INVALIDARG: return "invalid timestamp request";
		case PANUTIERRNO_NOTFOUND: return "the path disappeared";
		case PANUTIERRNO_UNSUPPORTEDOP: return "the filesystem cannot set timestamps";
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
	int64_t now = 0;
	int have_now = 0;

	for (int i = first; i < argc; i++) {
		const char* path = argv[i];

		if (!nexist(path)) {
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

			continue;
		}

		if (!have_now) {
			if (pcr_clock_centis(&now) != 0) {
				printf("pcrutils: touch: cannot read /dvc/time/clock\n");
				return -1;
			}

			have_now = 1;
		}

		inode_timestamp_t ts = {
			.atime = (time_t)now,
			.mtime = (time_t)now,
		};

		int32_t rc = settmsp(path, &ts, INODE_ATTR_ATIME | INODE_ATTR_MTIME);

		if (rc != 0) {
			const char* why = set_reason(rc);

			printf("pcrutils: touch: cannot set times on '%s': ", path);

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