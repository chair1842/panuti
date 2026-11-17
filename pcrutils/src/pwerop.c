/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <panuti/pwerop.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"pwerop - a pcrutils utility\n\n"
	"pwerop either reboots or shuts down the system if specified.\n"
	"if not specified, pwerop does nothing\n"
	"if many operations are specified (why), the order of precedence is this:\n"
	"  reboot, shutdown"
	"args:\n"
	"  -h - prints this help message\n"
	"  -s - specify shutdown\n"
	"  -r - specify reboot\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	// returns are here, i don't know what with me
	if (pcr_option_provided(argc, argv, "r")) {
		pwerop_reboot();
		
		printf("pcrutils: pwerop: reboot failed (how)\n");
		return -1;
	} else if (pcr_option_provided(argc, argv, "s")) {
		pwerop_shutdown();
		
		printf("pcrutils: pwerop: shutdown failed (how)\n");
		return -1;
	}

	printf("pcrutils: pwerop: you no specify, i no do\n");
	return 0;
}