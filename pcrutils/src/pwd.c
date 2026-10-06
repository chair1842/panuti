/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <panuti/syscall/syscallsf.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"pwd - a pcrutils utility\n\n"
	"pwd prints the current working directory of the creating process\n\n"
	"args (only the first argument is considered):\n"
	"  -h - prints this help message\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}
	
	char buf[1024];
	int r = panutisysf_getcwd(buf, sizeof(buf));
	if (r == 0) {
		printf("%s\n", buf);
		return 0;
	} else {
		printf("pcrutils: pwd: ERROR\n");
		return -1;
	}
}