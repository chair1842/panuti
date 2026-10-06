/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <pcrutils/pcrutils.h>

#define DIRNAME_MAX 128

static const char HELP[] =
	"dirname - a pcrutils utility\n\n"
	"dirname prints the directory a path lives in, everything before the\n"
	"final '/'\n\n"
	"usage:\n"
	"  dirname <path>\n\n"
	"args:\n"
	"  -h - prints this help message\n\n"
	"a path with no '/' in it belongs to the current directory, so '.'\n"
	"comes back for it\n\n"
	"examples:\n"
	"  dirname /tmp/junk.c\n"
	"  dirname junk.c\n";

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

		printf("pcrutils: dirname: invalid option '%s'\n", a + 1);
		return -1;
	}

	if (first == argc) {
		printf("pcrutils: dirname: no path given\n");
		return -1;
	}

	if (argc - first > 1) {
		printf("pcrutils: dirname: too many arguments\n");
		return -1;
	}

	const char* path = argv[first];

	size_t len = strlen(path);
	while (len > 1 && path[len - 1] == '/') {
		len--;
	}

	if (len == 0) {
		printf("pcrutils: dirname: '%s' has no directory in it\n", path);
		return -1;
	}

	size_t pos = (size_t)-1;
	for (size_t i = 0; i < len; i++) {
		if (path[i] == '/') {
			pos = i;
		}
	}

	if (pos == (size_t)-1) {
		printf(".\n");
		return 0;
	}

	if (pos == 0) {
		printf("/\n");
		return 0;
	}

	size_t plen = pos;
	while (plen > 1 && path[plen - 1] == '/') {
		plen--;
	}

	if (plen >= DIRNAME_MAX) {
		printf("pcrutils: dirname: the name is too long\n");
		return -1;
	}

	char out[DIRNAME_MAX];
	memcpy(out, path, plen);
	out[plen] = '\0';
	printf("%s\n", out);

	return 0;
}