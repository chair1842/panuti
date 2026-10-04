/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/mount.h>
#include <panuti/errno.h>

static const char* reason(int32_t rc) {
	switch (rc) {
		case PANUTIERRNO_PLAINSUCCESS: return "success";
		case PANUTIERRNO_PLAINERR: return "mount failed (already mounted, device too small, or not a valid filesystem)";
		case PANUTIERRNO_INVALIDADDR: return "not a valid user pointer";
		case PANUTIERRNO_NOTSUPPORTED: return "unknown filesystem type";
		case PANUTIERRNO_NOTFOUND: return "block device or mountpoint not found";
		case PANUTIERRNO_NOMEM: return "out of memory";
		case -1: return "filesystem scan read an invalid block";
		case -2: return "block layer I/O error";
		default: return NULL;
	}
}

static void print_help(void) {
	printf("mount - a pcrutils utility\n\n");
	printf("mount attaches a filesystem from a block device onto a directory\n\n");
	printf("usage:\n");
	printf("  mount <mountpoint> <fstype> <blkdev>\n\n");
	printf("fstype is one of: isofs, ext2\n\n");
	printf("blkdev is a registry path such as /dvc/cdrom0 or /dvc/pata0p1\n\n");
	printf("args:\n");
	printf("  -h - prints this help message\n\n");
	printf("examples:\n");
	printf("  mount /mnt ext2 /dvc/pata0p1\n");
	printf("  mount /mnt isofs /dvc/cdrom0\n");
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
				printf("pcrutils: mount: invalid option '%c'\n", a[j]);
				return -1;
			}
		}
	}

	if (first == argc) {
		printf("pcrutils: mount: no arguments given\n");
		print_help();
		return -1;
	}

	if (argc - first < 3) {
		printf("pcrutils: mount: need a mountpoint, a filesystem type and a block device\n");
		return -1;
	}

	if (argc - first > 3) {
		printf("pcrutils: mount: too many arguments\n");
		return -1;
	}

	const char* mountp = argv[first];
	const char* fstype = argv[first + 1];
	const char* blkdev = argv[first + 2];

	int32_t rc = mount(mountp, fstype, blkdev);

	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		const char* why = reason(rc);

		if (why) {
			printf("pcrutils: mount: %s %s (%s) failed: %s\n", mountp, fstype, blkdev, why);
		} else {
			printf("pcrutils: mount: %s %s (%s) failed: unknown error %d\n", mountp, fstype, blkdev, (int)rc);
		}

		return -1;
	}

	printf("mounted %s at %s\n", fstype, mountp);

	return 0;
}