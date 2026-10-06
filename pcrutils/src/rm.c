/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/inode_type.h>
#include <panuti/stat.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

#define RM_MAXDEPTH 64

static bool force = false;
static bool recurse = false;

static const char HELP[] =
	"rm - a pcrutils utility\n\n"
	"rm removes files and empty directories\n\n"
	"usage:\n"
	"  rm [-f] [-r] <path ...>\n\n"
	"args:\n"
	"  -f - do not complain about a path that is not there\n"
	"  -r - remove a directory's whole tree as well, empty or not\n"
	"  -h - prints this help message\n\n"
	"an empty directory comes off without any help; one that still holds\n"
	"something needs -r. a directory inside a mounted filesystem comes\n"
	"off only if the filesystem allows it\n\n"
	"examples:\n"
	"  rm /tmp/junk\n"
	"  rm -r /tmp/old\n";

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

// a directory with nothing in it but '.' and '..'
static bool is_empty_dir(const char* path) {
	int fd = handle_open(path);
	if (fd < 0) {
		return false;
	}

	bool empty = true;
	dirent_entry_t entry;
	int rc;

	while ((rc = readdir(fd, &entry)) == 0) {
		if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
			continue;
		}

		empty = false;
		break;
	}

	handle_close(fd);
	return empty;
}

// clear out a directory's contents, leaving it empty but in place. reports
// nothing itself; the caller decides whether to complain
static int clear_dir(const char* path, int depth) {
	if (depth > RM_MAXDEPTH) {
		return -1;
	}

	int fd = handle_open(path);
	if (fd < 0) {
		return -1;
	}

	int failed = 0;
	dirent_entry_t entry;
	int rc;

	while ((rc = readdir(fd, &entry)) == 0) {
		if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
			continue;
		}

		char* full = pcr_path_join(path, entry.name);
		if (!full) {
			failed = 1;
			continue;
		}

		if (entry.type == INODE_DIR) {
			if (clear_dir(full, depth + 1) != 0) {
				failed = 1;
			}

			if (panutisysf_unlink(full) != PANUTIERRNO_PLAINSUCCESS) {
				failed = 1;
			}
		} else if (panutisysf_unlink(full) != PANUTIERRNO_PLAINSUCCESS) {
			failed = 1;
		}

		free(full);
	}

	handle_close(fd);
	return failed ? -1 : 0;
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
		} else if (strcmp(a, "-r") == 0) {
			recurse = true;
		} else if (strcmp(a, "-d") == 0) {
			printf("pcrutils: rm: -d is gone; an empty directory comes off by itself\n");
			return -1;
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

		if (e.type == INODE_DIR && !is_empty_dir(path)) {
			if (recurse) {
				if (clear_dir(path, 0) != 0) {
					if (!force) {
						printf("pcrutils: rm: could not clear everything under '%s'\n", path);
					}
					failed = 1;
				}
			} else {
				if (!force) {
					printf("pcrutils: rm: cannot remove '%s': it holds something, give -r\n", path);
				}
				failed = 1;
				continue;
			}
		}

		int32_t rc = panutisysf_unlink(path);

		if (rc != PANUTIERRNO_PLAINSUCCESS && !force) {
			const char* why = reason(rc);

			printf("pcrutils: rm: cannot remove '%s': ", path);

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