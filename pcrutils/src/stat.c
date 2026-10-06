/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/stat.h>
#include <panuti/inode_type.h>
#include <panuti/errno.h>
#include <pcrutils/pcrutils.h>

static const char* type_label(inode_type_t type) {
	switch (type) {
		case INODE_DIR: return "directory";
		case INODE_FILE: return "file";
		case INODE_BLOCK: return "block device";
		case INODE_PIPE: return "pipe";
		default: return "unknown";
	}
}

static const char HELP[] =
	"stat - a pcrutils utility\n\n"
	"stat describes nodes in the registry without opening them\n\n"
	"usage:\n"
	"  stat [path ...]\n\n"
	"args:\n"
	"  -h - prints this help message\n";

static int stat_path(const char* path) {
	dirent_entry_t e;

	if (stat(path, &e) != 0) {
		printf("pcrutils: stat: %s: could not describe the node\n", path);
		return -1;
	}

	printf("  Node: %s\n", path);
	printf("  Name: %s\n", e.name);
	printf("  Type: %s\n", type_label(e.type));
	printf("  Size: %zu\n", e.size);

	return 0;
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

		for (int j = 1; a[j]; j++) {
			printf("pcrutils: stat: invalid option '%c'\n", a[j]);
			return -1;
		}
	}

	if (first == argc) {
		printf("pcrutils: stat: no path given\n");
		return -1;
	}

	int failed = 0;

	for (int i = first; i < argc; i++) {
		if (stat_path(argv[i]) != 0) {
			failed = 1;
		}

		if (i + 1 < argc) {
			printf("\n");
		}
	}

	return failed ? -1 : 0;
}
