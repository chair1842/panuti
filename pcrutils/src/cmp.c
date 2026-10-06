/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <pcrutils/pcrutils.h>

#define CMP_BUFSZ 128

static const char HELP[] =
	"cmp - a pcrutils utility\n\n"
	"cmp reads two files against each other, and says no more until they\n"
	"differ\n\n"
	"usage:\n"
	"  cmp <file a> <file b>\n\n"
	"args:\n"
	"  -h - prints this help message\n\n"
	"the first byte where the two part ways is reported, and the exit\n"
	"is not clean. a file that ends before the other is a difference too\n\n"
	"examples:\n"
	"  cmp /tmp/a /tmp/b\n";

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

		printf("pcrutils: cmp: invalid option '%s'\n", a + 1);
		return -1;
	}

	if (argc - first < 2) {
		printf("pcrutils: cmp: two files are needed\n");
		printf("%s", HELP);
		return -1;
	}

	if (argc - first > 2) {
		printf("pcrutils: cmp: too many arguments\n");
		return -1;
	}

	const char* patha = argv[first];
	const char* pathb = argv[first + 1];

	int fa = handle_open(patha);
	if (fa < 0) {
		printf("pcrutils: cmp: %s: %s\n", patha,
		       fa == (int)PANUTIERRNO_NOTFOUND ? "no such file" : "could not open it");
		return -1;
	}

	int fb = handle_open(pathb);
	if (fb < 0) {
		printf("pcrutils: cmp: %s: %s\n", pathb,
		       fb == (int)PANUTIERRNO_NOTFOUND ? "no such file" : "could not open it");
		handle_close(fa);
		return -1;
	}

	char bufa[CMP_BUFSZ];
	char bufb[CMP_BUFSZ];
	size_t na = 0;
	size_t nb = 0;
	unsigned long off = 0;
	int result = 0;

	while (na > 0 || nb > 0 || result >= 0) {
		if (na == 0) {
			int n = handle_read(fa, bufa, sizeof(bufa));
			if (n < 0) {
				printf("pcrutils: cmp: could not read %s\n", patha);
				result = -1;
				break;
			}
			na = (size_t)n;
		}

		if (nb == 0) {
			int n = handle_read(fb, bufb, sizeof(bufb));
			if (n < 0) {
				printf("pcrutils: cmp: could not read %s\n", pathb);
				result = -1;
				break;
			}
			nb = (size_t)n;
		}

		if (na == 0 || nb == 0) {
			if (na != nb) {
				printf("%s on %s at byte %lu\n",
				       na == 0 ? patha : pathb,
				       na == 0 ? "EOF" : "EOF",
				       off);
				result = -1;
			}
			break;
		}

		size_t m = na < nb ? na : nb;
		size_t i = 0;

		while (i < m) {
			if (bufa[i] != bufb[i]) {
				printf("%s %s differ: byte %lu\n", patha, pathb, off + i);
				result = -1;
				break;
			}
			i++;
		}

		if (result != 0) {
			break;
		}

		off += m;
		na -= m;
		nb -= m;

		if (na > 0) {
			memmove(bufa, bufa + m, na);
		}
		if (nb > 0) {
			memmove(bufb, bufb + m, nb);
		}
	}

	handle_close(fa);
	handle_close(fb);
	return result == 0 ? 0 : -1;
}