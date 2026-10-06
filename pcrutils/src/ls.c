/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/handle.h>
#include <panuti/stat.h>
#include <panuti/dirent.h>
#include <panuti/inode_type.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"ls - a pcrutils utility\n\n"
	"ls lists the contents of directories\n\n"
	"args:\n"
	"  -h - prints this help message\n"
	"  -a - do not ignore entries starting with '.'\n"
	"  -p - do not print the type column\n"
	"  <path> - directory to list (defaults to the current directory)\n";

static int show_hidden = 0;
static int no_type = 0;

static int is_hidden(const char* name) {
	return name[0] == '.';
}

static const char* type_label(inode_type_t type) {
	switch (type) {
		case INODE_DIR: return "dir";
		case INODE_FILE: return "file";
		case INODE_BLOCK: return "block";
		case INODE_PIPE: return "pipe";
		default: return "other";
	}
}

static void print_entry(const dirent_entry_t* e, size_t maxw) {
	size_t n = strlen(e->name);
	int slash = (e->type == INODE_DIR) ? 1 : 0;

	if (no_type) {
		printf("%s%s\n", e->name, slash ? "/" : "");
		return;
	}

	size_t w = n + (size_t)slash;
	int pad = (int)maxw - (int)w;

	printf("%s%s%*s%s\n", e->name, slash ? "/" : "", pad + 1, "", type_label(e->type));
}

static size_t dir_max_width(const char* path) {
	int fd = handle_open(path);
	if (fd < 0) {
		return (size_t)-1;
	}

	size_t w = 0;
	dirent_entry_t entry;
	int rc;
	while ((rc = readdir(fd, &entry)) == 0) {
		if (!show_hidden && is_hidden(entry.name)) {
			continue;
		}
		size_t n = strlen(entry.name);
		if (entry.type == INODE_DIR) {
			n++;
		}
		if (n > w) {
			w = n;
		}
	}

	handle_close(fd);
	return w;
}

static int list_dir(const char* path, size_t maxw) {
	int fd = handle_open(path);
	if (fd < 0) {
		printf("pcrutils: ls: %s: cannot open directory\n", path);
		return -1;
	}

	dirent_entry_t entry;
	int rc;
	while ((rc = readdir(fd, &entry)) == 0) {
		if (!show_hidden && is_hidden(entry.name)) {
			continue;
		}
		print_entry(&entry, maxw);
	}

	handle_close(fd);
	return 0;
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	int i = 1;
	for (; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			break;
		}

		if (strcmp(a, "-a") == 0) {
			show_hidden = 1;
		} else if (strcmp(a, "-p") == 0) {
			no_type = 1;
		} else {
			printf("pcrutils: ls: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	int failed = 0;
	if (i == argc) {
		char* default_paths[1] = { "." };
		for (int k = 0; k < 1; k++) {
			size_t maxw = no_type ? 0 : dir_max_width(default_paths[k]);
			if (maxw == (size_t)-1) {
				failed = 1;
				continue;
			}
			if (list_dir(default_paths[k], maxw) != 0) {
				failed = 1;
			}
		}
		return failed ? -1 : 0;
	}

	for (; i < argc; i++) {
		size_t maxw = no_type ? 0 : dir_max_width(argv[i]);
		if (maxw == (size_t)-1) {
			printf("pcrutils: ls: %s: cannot open directory\n", argv[i]);
			failed = 1;
			continue;
		}
		if (list_dir(argv[i], maxw) != 0) {
			failed = 1;
		}
	}

	return failed ? -1 : 0;
}