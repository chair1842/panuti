/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdint.h>

#include <panuti/errno.h>
#include <panuti/process.h>

#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"kill - a pcrutils utility\n\n"
	"kill asks one or more processes to terminate\n\n"
	"args:\n"
	"  -h - prints this help message\n"
	"  <pid> - process to terminate; more than one pid may be given\n";

static int kill_one(const char* arg) {
	bool ok;
	long pid = pcr_parse_long(arg, &ok);

	if (!ok || pid <= 0) {
		printf("pcrutils: kill: %s: invalid pid\n", arg);
		return -1;
	}

	int rc = kill((pid_t)pid);

	if ((uint32_t)rc == PANUTIERRNO_NOTFOUND) {
		printf("pcrutils: kill: %s: no such process\n", arg);
		return -1;
	}

	if ((uint32_t)rc == PANUTIERRNO_UNSUPPORTEDOP) {
		printf("pcrutils: kill: %s: process cannot be terminated\n", arg);
		return -1;
	}

	if (rc != 0) {
		printf("pcrutils: kill: %s: could not signal process\n", arg);
		return -1;
	}

	return 0;
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	if (argc < 2) {
		printf("pcrutils: kill: missing pid, add a darned pid\n");
		return -1;
	}

	int status = 0;

	for (int i = 1; i < argc; i++) {
		if (kill_one(argv[i]) != 0) {
			status = -1;
		}
	}

	return status;
}
