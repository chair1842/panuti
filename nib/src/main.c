/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdio.h>

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("nib: no file provided\n");
		return -1;
	} else if (argc > 2) {
		printf("nib: ignoring other args\n");
	}
	
	return 0;
}