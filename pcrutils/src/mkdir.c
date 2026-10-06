/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/registry.h>
#include <panuti/stat.h>
#include <panuti/inode_type.h>
#include <panuti/errno.h>
#include <pcrutils/pcrutils.h>

#define MKDIR_PATH_MAX 128

static int make_parents = 0;

// mkdir reports most path-level rejections as a raw -1 rather than a
// PANUTIERRNO_* code, and the parent-in-a-mount case comes back as
// UNSUPPORTEDOP. Both have to be named or they surface as a bare number.
static const char* reason(int32_t rc) {
	switch (rc) {
		case PANUTIERRNO_PLAINSUCCESS: return "success";
		case PANUTIERRNO_INVALIDADDR: return "not a valid user pointer";
		case PANUTIERRNO_UNSUPPORTEDOP: return "the filesystem is mounted read-only";
		case PANUTIERRNO_NOTFOUND: return "no such directory";
		case PANUTIERRNO_EXISTS: return "already exists";
		case PANUTIERRNO_NOMEM: return "out of memory";
		case -1: return "could not create the directory (no such parent, name already taken, or the name is too long)";
		default: return NULL;
	}
}

static const char HELP[] =
	"mkdir - a pcrutils utility\n\n"
	"mkdir creates a directory\n\n"
	"usage:\n"
	"  mkdir [-p] <path>\n\n"
	"args:\n"
	"  -p - create parent directories as needed, and do not complain\n"
	"       if the directory already exists\n"
	"  -h - prints this help message\n\n"
	"with -p, every component but the last may already exist\n\n"
	"a directory inside a mounted filesystem cannot be created:\n"
	"ext2 is read-only and no filesystem implements directory creation\n\n"
	"examples:\n"
	"  mkdir /tmp\n"
	"  mkdir -p /a/b/c\n";

// -p tolerates a component that already exists, but only if it is a directory.
// nexist cannot tell a file from a directory, so stat has to be asked.
static int is_directory(const char* path) {
	dirent_entry_t e;

	if (stat(path, &e) != 0) {
		return 0;
	}

	return e.type == INODE_DIR;
}

// creates one directory, reporting failures. returns 0 on success.
static int make_one(const char* path) {
	int32_t rc = mkdir(path);

	if (rc == PANUTIERRNO_PLAINSUCCESS) {
		return 0;
	}

	const char* why = reason(rc);

	printf("pcrutils: mkdir: cannot create directory '%s': ", path);

	if (why) {
		printf("%s\n", why);
	} else {
		printf("unknown error %d\n", (int)rc);
	}

	return -1;
}

// with -p, walk the path from the root outwards creating each component. The
// kernel only ever creates the final component, so the prefixes go one by one.
static int make_path(const char* path) {
	char buf[MKDIR_PATH_MAX];
	size_t len = strlen(path);
	int absolute = (len > 0 && path[0] == '/');
	size_t i;

	if (len >= sizeof(buf)) {
		printf("pcrutils: mkdir: cannot create directory '%s': path too long\n", path);
		return -1;
	}

	memcpy(buf, path, len + 1);

	// normalise repeated slashes and drop a trailing one so each prefix below
	// is something the kernel can resolve
	size_t out = absolute ? 1 : 0;

	for (i = (size_t)(absolute ? 1 : 0); i < len; i++) {
		if (buf[i] == '/' && buf[out - 1] == '/') {
			continue;
		}

		buf[out++] = buf[i];
	}

	while (out > 1 && buf[out - 1] == '/') {
		out--;
	}

	buf[out] = '\0';

	if (out == 0) {
		printf("pcrutils: mkdir: cannot create directory '%s': no name given\n", path);
		return -1;
	}

	// the root always exists, and mkdir on it would be a no-op at best
	if (absolute && out == 1) {
		if (nexist("/")) {
			return 0;
		}

		printf("pcrutils: mkdir: cannot create directory '/': already exists\n");
		return -1;
	}

	for (i = 1; i < out; i++) {
		if (buf[i] != '/') {
			continue;
		}

		// keep the prefix itself for this mkdir call
		char saved = buf[i];

		buf[i] = '\0';

		// a prefix that is already a directory is fine; one that exists as
		// something else, or does not exist and could not be created, is not
		if (mkdir(buf) != PANUTIERRNO_PLAINSUCCESS) {
			if (!is_directory(buf)) {
				printf("pcrutils: mkdir: cannot create directory '%s': no such parent\n", buf);
				return -1;
			}
		}

		buf[i] = saved;
	}

	// the final component has to actually be created, so a pre-existing name is
	// only tolerated when -p asked for it *and* it is already a directory
	if (is_directory(buf)) {
		if (make_parents) {
			return 0;
		}

		printf("pcrutils: mkdir: cannot create directory '%s': already exists\n", buf);
		return -1;
	}

	if (nexist(buf)) {
		printf("pcrutils: mkdir: cannot create directory '%s': a file of that name already exists\n", buf);
		return -1;
	}

	return make_one(buf);
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

		if (strcmp(a, "-p") != 0) {
			printf("pcrutils: mkdir: invalid option '%s'\n", a + 1);
			return -1;
		}

		make_parents = 1;
	}

	if (first == argc) {
		printf("pcrutils: mkdir: no path given\n");
		printf("%s", HELP);
		return -1;
	}

	if (argc - first > 1) {
		printf("pcrutils: mkdir: too many arguments\n");
		return -1;
	}

	const char* path = argv[first];

	if (path[0] == '\0') {
		printf("pcrutils: mkdir: no path given\n");
		return -1;
	}

	if (make_parents) {
		return make_path(path);
	}

	// without -p the kernel still creates only the final component, and it
	// rejects a name collision, so a pre-check would only duplicate its work
	return make_one(path);
}