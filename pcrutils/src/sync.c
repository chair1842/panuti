/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <panuti/errno.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"sync - a pcrutils utility\n\n"
	"sync flushes all mounted filesystems to their block devices\n\n"
	"usage:\n"
	"  sync\n\n"
	"args:\n"
	"  -h - prints this help message\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	if (argc > 1) {
		printf("pcrutils: sync: takes no arguments\n");
		return -1;
	}

	int32_t rc = panutisysf_sync();

	if (rc != PANUTIERRNO_PLAINSUCCESS) {
		printf("pcrutils: sync: failed: unknown error %d\n", (int)rc);
		return -1;
	}

	return 0;
}
