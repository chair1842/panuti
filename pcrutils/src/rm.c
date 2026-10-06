/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/inode_type.h>
#include <panuti/stat.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static bool force = false;
static bool dirs = false;

static const char HELP[] =
	"rm - a pcrutils utility\n\n"
	"rm removes nodes from the registry\n\n"
	"usage:\n"
	"  rm [-f] [-d] <path ...>\n\n"
	"args:\n"
	"  -f - do not complain about a path that is not there\n"
	"  -d - remove a directory as well as a file. registry directories\n"
	"       can be taken this way; a directory inside a mounted filesystem\n"
	"       may not be, that is up to the filesystem\n"
	"  -h - prints this help message\n\n"
	"examples:\n"
	"  rm /tmp/junk\n"
	"  rm -d /tmp/old\n";

static const char* reason(int32_t rc) {
	switch (rc) {
		case PANUTIERRNO_PLAINSUCCESS: return "success";
		case PANUTIERRNO_INVALIDADDR: return "not a valid user pointer";
		case PANUTIERRNO_NOTFOUND: return "no such file or directory";
		case PANUTIERRNO_UNSUPPORTEDOP: return "the filesystem does not allow it";
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

		if (strcmp(a, "-f") == 0) {
			force = true;
		} else if (strcmp(a, "-d") == 0) {
			dirs = true;
		} else {
			printf("pcrutils: rm: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	if (first == argc) {
		printf("pcrutils: rm: no path given\n");
		printf("%s", HELP);
		return -1;
	}

	int failed = 0;

	for (int i = first; i < argc; i++) {
		const char* path = argv[i];

		dirent_entry_t e;

		if (stat(path, &e) != 0) {
			if (!force) {
				printf("pcrutils: rm: cannot remove '%s': no such file or directory\n", path);
				failed = 1;
			}
			continue;
		}

		if (e.type == INODE_DIR && !dirs) {
			printf("pcrutils: rm: cannot remove '%s': it is a directory, give -d\n", path);
			failed = 1;
			continue;
		}

		int32_t rc = panutisysf_unlink(path);

		if (rc != PANUTIERRNO_PLAINSUCCESS && !force) {
			const char* why = reason(rc);

			if (why) {
				printf("pcrutils: rm: cannot remove '%s': %s\n", path, why);
			} else {
				printf("pcrutils: rm: cannot remove '%s': unknown error %d\n", path, (int)rc);
			}

			failed = 1;
		}
	}

	return failed ? -1 : 0;
}