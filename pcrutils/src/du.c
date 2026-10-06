/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <panuti/handle.h>
#include <panuti/inode_type.h>
#include <panuti/stat.h>
#include <pcrutils/pcrutils.h>

#define DU_MAXDEPTH 64

static bool summarize = false;
static bool human = false;

static const char HELP[] =
	"du - a pcrutils utility\n\n"
	"du adds up the sizes of the files under the paths it is given,\n"
	"and reports the total to out0,\n"
	"one line per directory unless -s asks for just the paths themselves\n\n"
	"usage:\n"
	"  du [-s] [-H] <path ...>\n\n"
	"args:\n"
	"  -s - print only the total for each path given, not the subtotals of every directory under it\n"
	"  -H - print the sizes in K, M and G instead of plain bytes\n"
	"  -h - prints this help message\n\n"
	"the current directory is sized when no path is given\n\n"
	"examples:\n"
	"  du /tmp\n"
	"  du -H -s /cd/usr\n";

static size_t dir_total(const char* path, int depth, bool summarize);

// the size of one node. only directories are walked into; a directory does
// not count its own node, only what it holds
static size_t node_size(const dirent_entry_t* e) {
	return e->type == INODE_DIR ? 0 : e->size;
}

static void human_size(unsigned long v, char* out) {
	static const char* units[] = {"", "K", "M", "G", "T"};
	int u = 0;

	while (v >= 1024 && u < 4) {
		v /= 1024;
		u++;
	}

	char num[16];
	int n = pcr_uformat(v, 10, num);
	memcpy(out, num, (size_t)n);
	out[n] = units[u][0];
	out[n + 1] = '\0';
}

// sums the contents of one directory, printing each subdirectory's own total
// as it finishes unless -s is on
static size_t sum_dir_contents(const char* path, int depth, bool summarize) {
	int fd = handle_open(path);
	if (fd < 0) {
		printf("pcrutils: du: %s: cannot open directory\n", path);
		return 0;
	}

	size_t total = 0;
	dirent_entry_t entry;
	int rc;

	while ((rc = readdir(fd, &entry)) == 0) {
		if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
			continue;
		}

		char* full = pcr_path_join(path, entry.name);
		if (!full) {
			handle_close(fd);
			return total;
		}

		total += entry.type == INODE_DIR
			? dir_total(full, depth + 1, summarize)
			: node_size(&entry);

		free(full);
	}

	handle_close(fd);
	return total;
}

static size_t dir_total(const char* path, int depth, bool summarize) {
	size_t total = sum_dir_contents(path, depth, summarize);

	if (depth <= DU_MAXDEPTH && !summarize) {
		if (human) {
			char h[16];
			human_size((unsigned long)total, h);
			printf("%s\t%s\n", h, path);
		} else {
			printf("%lu\t%s\n", (unsigned long)total, path);
		}
	}

	return total;
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

		if (strcmp(a, "-s") == 0) {
			summarize = true;
		} else if (strcmp(a, "-H") == 0) {
			human = true;
		} else {
			printf("pcrutils: du: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	int failed = 0;

	if (first == argc) {
		dirent_entry_t e;

		if (stat(".", &e) == 0 && e.type == INODE_DIR) {
			size_t total = sum_dir_contents(".", 0, summarize);

			if (human) {
				char h[16];
				human_size((unsigned long)total, h);
				printf("%s\t%s\n", h, ".");
			} else {
				printf("%lu\t%s\n", (unsigned long)total, ".");
			}
		} else {
			failed = 1;
		}
	}

	for (int i = first; i < argc; i++) {
		dirent_entry_t e;

		if (stat(argv[i], &e) != 0) {
			printf("pcrutils: du: %s: no such file or directory\n", argv[i]);
			failed = 1;
			continue;
		}

		size_t total = e.type == INODE_DIR
			? dir_total(argv[i], 0, summarize)
			: node_size(&e);

		if (summarize) {
			if (human) {
				char h[16];
				human_size((unsigned long)total, h);
				printf("%s\t%s\n", h, argv[i]);
			} else {
				printf("%lu\t%s\n", (unsigned long)total, argv[i]);
			}
		}
	}

	return failed ? -1 : 0;
}