/* SPDX-License-Identifier: BSD-3-Clause */

#include "eval.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char HELP[] =
	"calc - a console calculator\n\n"
	"calc solves an integer arithmetic problem and prints the result\n\n"
	"usage:\n"
	"  calc <problem>\n\n"
	"example:\n"
	"  calc \"5 + 5\"\n\n"
	"the problem takes + - * / % and parentheses\n";

// the problem is normally one quoted argument, but the words are joined back
// together so `calc 5 + 5` solves the same thing as `calc "5 + 5"`
static char* join_args(int argc, char** argv) {
	size_t len = 0;

	for (int i = 1; i < argc; i++) {
		len += strlen(argv[i]) + 1;
	}

	char* buf = malloc(len);
	if (!buf) {
		return NULL;
	}

	char* p = buf;

	for (int i = 1; i < argc; i++) {
		if (i > 1) {
			*p++ = ' ';
		}

		size_t n = strlen(argv[i]);
		memcpy(p, argv[i], n);
		p += n;
	}

	*p = '\0';
	return buf;
}

static int argument_mode(int argc, char** argv) {
	char* problem = join_args(argc, argv);
	if (!problem) {
		printf("calc: out of memory\n");
		return 1;
	}

	long result = 0;
	int rc = calc_eval(problem, &result);
	free(problem);

	if (rc != CALC_OK) {
		printf("calc: %s\n", calc_eval_strerror(rc));
		return 1;
	}

	printf("%ld\n", result);
	return 0;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("calc: interactive mode is not here yet\n");
		printf("usage: calc <problem>\n");
		return 1;
	}

	if (argc == 2 && strcmp(argv[1], "-h") == 0) {
		printf("%s", HELP);
		return 0;
	}

	return argument_mode(argc, argv);
}
