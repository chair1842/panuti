/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <panuti/handle.h>
#include <panuti/inode_type.h>
#include <panuti/stat.h>
#include <pcrutils/pcrutils.h>

#define FIND_MAXDEPTH 64

static const char* name_pat = NULL;
static inode_type_t type_pat = INODE_NONE;
static bool have_type = false;

static const char HELP[] =
	"find - a pcrutils utility\n\n"
	"find walks a directory tree, printing the path of every node it comes to\n\n"
	"usage:\n"
	"  find <path ...> [-name pattern] [-type f|d]\n\n"
	"args:\n"
	"  -name pattern - print only nodes whose name matches, with * ? and\n"
	"                  [a-z] the same way ls looks at them\n"
	"  -type f - print only files\n"
	"  -type d - print only directories\n"
	"  -h - prints this help message\n\n"
	"the current directory is walked when no path is given\n\n"
	"examples:\n"
	"  find /tmp -name '*.c'\n"
	"  find / -type d\n";

static bool hidden_dot(const char* name) {
	return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static bool want_entry(const char* name, inode_type_t type) {
	if (name_pat && !pcr_glob_match(name_pat, name)) {
		return false;
	}

	if (have_type && type != type_pat) {
		return false;
	}

	return true;
}

// walks one directory, printing matches and descending into subdirectories.
// returns the number of failures met along the way
static int walk(const char* path, int depth) {
	if (depth > FIND_MAXDEPTH) {
		printf("pcrutils: find: %s: too deep\n", path);
		return 1;
	}

	int fd = handle_open(path);
	if (fd < 0) {
		printf("pcrutils: find: %s: cannot open directory\n", path);
		return 1;
	}

	int failed = 0;
	dirent_entry_t entry;
	int rc;

	while ((rc = readdir(fd, &entry)) == 0) {
		if (hidden_dot(entry.name)) {
			continue;
		}

		char* full = pcr_path_join(path, entry.name);
		if (!full) {
			failed++;
			continue;
		}

		if (want_entry(entry.name, entry.type)) {
			printf("%s\n", full);
		}

		if (entry.type == INODE_DIR) {
			failed += walk(full, depth + 1);
		}

		free(full);
	}

	handle_close(fd);
	return failed;
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

		if (strcmp(a, "-name") == 0 || strcmp(a, "-type") == 0) {
			bool is_name = (a[1] == 'n');

			if (i + 1 >= argc) {
				printf("pcrutils: find: %s needs a value\n", a);
				return -1;
			}

			const char* value = argv[++i];

			if (is_name) {
				name_pat = value;
			} else if (strcmp(value, "f") == 0) {
				type_pat = INODE_FILE;
				have_type = true;
			} else if (strcmp(value, "d") == 0) {
				type_pat = INODE_DIR;
				have_type = true;
			} else {
				printf("pcrutils: find: -type takes f or d, not '%s'\n", value);
				return -1;
			}
		} else {
			printf("pcrutils: find: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	// the current directory is walked when no path is given
	if (first == argc) {
		return walk(".", 0) ? -1 : 0;
	}

	if (first + 1 == argc) {
		return walk(argv[first], 0) ? -1 : 0;
	}

	int failed = 0;

	for (int i = first; i < argc; i++) {
		failed += walk(argv[i], 0);
	}

	return failed ? -1 : 0;
}