/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/mount.h>
#include <panuti/errno.h>

// unmount only ever distinguishes three outcomes. the mount utility carries a
// wider table because mounting can fail in more ways; this one cannot.
static const char* reason(int32_t rc) {
	switch (rc) {
		case PANUTIERRNO_PLAINSUCCESS: return "success";
		case PANUTIERRNO_INVALIDADDR: return "not a valid user pointer";
		case PANUTIERRNO_NOTFOUND: return "no filesystem mounted there";
		default: return NULL;
	}
}

static void print_help(void) {
	printf("umount - a pcrutils utility\n\n");
	printf("umount detaches the filesystem attached to a directory\n\n");
	printf("usage:\n");
	printf("  umount <mountpoint>\n\n");
	printf("args:\n");
	printf("  -h - prints this help message\n\n");
	printf("example:\n");
	printf("  umount /mnt\n");
}

int main(int argc, char** argv) {
	int first = argc;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			first = i;
			break;
		}

		for (int j = 1; a[j]; j++) {
			if (a[j] == 'h') {
				print_help();
				return 0;
			} else {
				printf("pcrutils: umount: invalid option '%c'\n", a[j]);
				return -1;
			}
		}
	}

	if (first == argc) {
		printf("pcrutils: umount: no mountpoint given\n");
		print_help();
		return -1;
	}

	if (argc - first > 1) {
		printf("pcrutils: umount: too many arguments\n");
		return -1;
	}

	const char* mountp = argv[first];

	int32_t rc = unmount(mountp);

	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		const char* why = reason(rc);

		if (why) {
			printf("pcrutils: umount: %s failed: %s\n", mountp, why);
		} else {
			printf("pcrutils: umount: %s failed: unknown error %d\n", mountp, (int)rc);
		}

		return -1;
	}

	printf("unmounted %s\n", mountp);

	return 0;
}