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

#define CP_BUFSZ 128

static bool recurse = false;

static const char HELP[] =
	"cp - a pcrutils utility\n\n"
	"cp copies files, or with -r whole directory trees, out over a\n"
	"second set of names\n\n"
	"usage:\n"
	"  cp [-r] <src ...> <dest>\n\n"
	"args:\n"
	"  -r - copy directories as well, making the matching tree\n"
	"  -h - prints this help message\n\n"
	"a destination that is an existing directory takes each source under\n"
	"its own name. a file that is not there yet is created on the spot,\n"
	"and one that is already there is wiped and rewritten\n\n"
	"examples:\n"
	"  cp /tmp/a /tmp/b\n"
	"  cp -r /tmp /cd/usr/garbage\n";

// the last component of a path, after any trailing slashes
static const char* base_name(const char* path, size_t* len_out) {
	size_t len = strlen(path);

	while (len > 1 && path[len - 1] == '/') {
		len--;
	}

	const char* base = path;
	for (size_t i = 0; i < len; i++) {
		if (path[i] == '/') {
			base = path + i + 1;
		}
	}

	*len_out = (size_t)((path + len) - base);
	return base;
}

static int is_directory(const char* path) {
	dirent_entry_t e;

	if (stat(path, &e) != 0) {
		return 0;
	}

	return e.type == INODE_DIR;
}

static void report_copy(const char* from, const char* to, const char* what) {
	printf("pcrutils: cp: cannot copy '%s' to '%s': %s\n", from, to, what);
}

// copy one file onto another, creating the destination if it is missing
static int copy_file(const char* src, const char* dst) {
	if (strcmp(src, dst) == 0) {
		report_copy(src, dst, "they are the same file");
		return -1;
	}

	int in = handle_open(src);
	if (in < 0) {
		report_copy(src, dst, in == (int)PANUTIERRNO_NOTFOUND ? "no such source" : "could not open the source");
		return -1;
	}

	int out = pcr_open_or_create(dst);
	if (out < 0) {
		report_copy(src, dst, out == (int)PANUTIERRNO_NOTFOUND ? "no such place to put it" : "could not open the destination");
		handle_close(in);
		return -1;
	}

	// a fresh copy always wipes an existing destination
	uint64_t zero = 0;
	panutisysf_resize(out, &zero);

	int failed = 0;
	char buf[CP_BUFSZ];

	for (;;) {
		int n = handle_read(in, buf, sizeof(buf));
		if (n < 0) {
			report_copy(src, dst, "could not read the source");
			failed = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		if (pcr_handle_write_all(out, buf, (size_t)n) != 0) {
			report_copy(src, dst, "could not write the destination");
			failed = 1;
			break;
		}
	}

	handle_close(in);
	handle_close(out);
	return failed ? -1 : 0;
}

// create one directory, tolerating that it already exists
static int make_dir(const char* path) {
	if (is_directory(path)) {
		return 0;
	}

	uint32_t rc = panutisysf_mkdir(path);

	if (rc != PANUTIERRNO_PLAINSUCCESS && !is_directory(path)) {
		report_copy(path, "", "could not make the destination directory");
		return -1;
	}

	return 0;
}

// copy a directory tree, src -> dst, both paths already known to be ours
static int copy_tree(const char* src, const char* dst) {
	int sfd = handle_open(src);
	if (sfd < 0) {
		report_copy(src, dst, "could not open the source directory");
		return -1;
	}

	int failed = 0;
	dirent_entry_t entry;
	int rc;

	while ((rc = readdir(sfd, &entry)) == 0) {
		if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
			continue;
		}

		char* s = pcr_path_join(src, entry.name);
		char* d = pcr_path_join(dst, entry.name);
		if (!s || !d) {
			free(s);
			free(d);
			failed = 1;
			continue;
		}

		if (entry.type == INODE_DIR) {
			if (make_dir(d) != 0 || copy_tree(s, d) != 0) {
				failed = 1;
			}
		} else if (copy_file(s, d) != 0) {
			failed = 1;
		}

		free(s);
		free(d);
	}

	handle_close(sfd);
	return failed ? -1 : 0;
}

// copy one source onto a target that was given on the command line
static int copy_source(const char* src, const char* dest, int dest_is_dir) {
	dirent_entry_t e;

	if (stat(src, &e) != 0) {
		report_copy(src, dest, "no such source");
		return -1;
	}

	if (e.type == INODE_DIR) {
		if (!recurse) {
			report_copy(src, dest, "it is a directory, give -r");
			return -1;
		}

		if (dest_is_dir) {
			size_t nlen;
			const char* bname = base_name(src, &nlen);
			char buf[CP_BUFSZ];
			if (nlen >= sizeof(buf)) {
				report_copy(src, dest, "the name is too long");
				return -1;
			}
			memcpy(buf, bname, nlen);
			buf[nlen] = '\0';
			char* target = pcr_path_join(dest, buf);
			if (!target) {
				return -1;
			}
			int r = make_dir(target) != 0 || copy_tree(src, target) != 0;
			free(target);
			return r ? -1 : 0;
		}

		// one source, one destination not yet a directory: the destination
		// becomes the copy of the source's whole tree
		if (make_dir(dest) != 0) {
			return -1;
		}

		return copy_tree(src, dest);
	}

	if (dest_is_dir) {
		size_t nlen;
		const char* bname = base_name(src, &nlen);
		char buf[CP_BUFSZ];
		if (nlen >= sizeof(buf)) {
			report_copy(src, dest, "the name is too long");
			return -1;
		}
		memcpy(buf, bname, nlen);
		buf[nlen] = '\0';
		char* target = pcr_path_join(dest, buf);
		if (!target) {
			return -1;
		}
		int r = copy_file(src, target);
		free(target);
		return r;
	}

	return copy_file(src, dest);
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

		if (strcmp(a, "-r") != 0) {
			printf("pcrutils: cp: invalid option '%s'\n", a + 1);
			return -1;
		}

		recurse = true;
	}

	int nsources = argc - first - 1;
	if (nsources < 1) {
		printf("pcrutils: cp: a source and a destination are needed\n");
		printf("%s", HELP);
		return -1;
	}

	if (nsources < 0) {
		printf("pcrutils: cp: a destination is needed\n");
		return -1;
	}

	const char* dest = argv[argc - 1];
	int dest_is_dir = is_directory(dest);

	if (nsources > 1 && !dest_is_dir) {
		printf("pcrutils: cp: several sources need a directory to land in\n");
		return -1;
	}

	int failed = 0;

	for (int i = first; i < argc - 1; i++) {
		if (copy_source(argv[i], dest, dest_is_dir) != 0) {
			failed = 1;
		}
	}

	return failed ? -1 : 0;
}