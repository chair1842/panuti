/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <panuti/handle.h>
#include <panuti/errno.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"clear - a pcrutils utility\n\n"
	"clear writes a form feed to the console, clearing the screen\n\n"
	"args (only the first argument is considered):\n"
	"  -h - prints this help message\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	int fd = handle_open("/dvc/console");
	if (fd < 0) {
		printf("pcrutils: clear: could not open /dvc/console\n");
		return -1;
	}

	char ff = '\f';
	int r = handle_write(fd, &ff, 1);
	handle_close(fd);

	if (r <= 0) {
		printf("pcrutils: clear: write to the console failed\n");
		return -1;
	}

	return 0;
}