/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <panuti/syscall/syscallsf.h>

int main(int argc, char** argv) {
	if (argc > 1) {
		if (strcmp(argv[1], "-h") == 0) {
			printf("pwd - a pcrutils utility\n\n");
			printf("pwd prints the current working directory of the creating process\n\n");
			printf("args (only the first argument is considered):\n");
			printf("  -h - prints this help message\n");
			
			return 1;
		}
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