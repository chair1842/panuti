/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <pcrutils/pcrutils.h>

#define BASENAME_MAX 256

static const char HELP[] =
	"basename - a pcrutils utility\n\n"
	"basename prints the last component of a path, the part after the\n"
	"final '/'\n\n"
	"usage:\n"
	"  basename <path> [suffix]\n\n"
	"args:\n"
	"  -h - prints this help message\n\n"
	"a suffix that the component ends with is taken off, the way .c files\n"
	"are told from their directory\n\n"
	"examples:\n"
	"  basename /tmp/junk.c\n"
	"  basename /tmp/junk.c .c\n";

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

		printf("pcrutils: basename: invalid option '%s'\n", a + 1);
		return -1;
	}

	if (first == argc) {
		printf("pcrutils: basename: no path given\n");
		return -1;
	}

	if (argc - first > 2) {
		printf("pcrutils: basename: too many arguments\n");
		return -1;
	}

	const char* path = argv[first];

	size_t len = strlen(path);
	while (len > 1 && path[len - 1] == '/') {
		len--;
	}

	if (len == 0) {
		printf("pcrutils: basename: '%s' has no name in it\n", path);
		return -1;
	}

	const char* base = path;
	for (size_t i = 0; i < len; i++) {
		if (path[i] == '/') {
			base = path + i + 1;
		}
	}

	size_t base_len = (size_t)((path + len) - base);

	if (argc - first == 2) {
		const char* suffix = argv[first + 1];
		size_t suffix_len = strlen(suffix);

		if (suffix_len > 0 && base_len > suffix_len &&
		    strncmp(base + base_len - suffix_len, suffix, suffix_len) == 0) {
			base_len -= suffix_len;
		}
	}

	if (base_len == 0 || base_len >= BASENAME_MAX) {
		printf("pcrutils: basename: the name is too long\n");
		return -1;
	}

	char out[BASENAME_MAX];
	memcpy(out, base, base_len);
	out[base_len] = '\0';
	printf("%s\n", out);

	return 0;
}