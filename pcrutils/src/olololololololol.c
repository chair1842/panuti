/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"olololololololol - a pcrutils \"utility\"\n\n"
	"why would you need help for olololololololol\n"
	"stinky.\n";

static const char TALK[] =
	"olololololololol - a pcrutils \"utility\"\n\n"
	"the only ways you could have discovered this was by:\n"
	"  - ls /usr/bin\n"
	"  - word of mouth\n"
	"  - looking through pcrutils source code\n\n"
	"if you just typed olololololololol by random on the panuti terminal,\n"
	"you have just struck a 1 in 100 quintillion chance (a naive estimate)\n"
	"you have a lesser chance at this than getting a wife (or husband)\n"
	"there's blablubing way you did this\n\n"
	"i, char1842, will never tell anyone about ololololololol unless they know about it.\n"
	"so, i thank you for being very olololololololol\n"
	"thank you for coming to my ololololololololol talk\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	printf("%s", TALK);

	// shhh, its the calculator thing...
	return 80085;
}