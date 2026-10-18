/* SPDX-License-Identifier: BSD-3-Clause */

#include "eval.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <panuti/stream.h>

#define CALC_LINE_MAX 256

static const char HELP[] =
	"calc - a console calculator\n\n"
	"calc solves a math problem\n"
	"integer erithmetic is the only thing supported rn\n\n"
	"usage:\n"
	"  calc <problem>   solve one problem and print the result\n"
	"  calc             start the interactive interface\n\n"
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

static void trim_trailing(char* s) {
	size_t n = strlen(s);

	while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
		s[--n] = '\0';
	}
}

static int interactive_mode(void) {
	char line[CALC_LINE_MAX];

	printf("calc - type a problem, or 'quit' to leave\n");

	for (;;) {
		printf("> ");

		int n = stream_read(0, line, sizeof(line));
		if (n < 0) {
			printf("\ncalc: could not read the input stream\n");
			return 1;
		}

		trim_trailing(line);

		if (line[0] == '\0') {
			continue;
		}

		if (strcmp(line, "quit") == 0) {
			break;
		}

		long result = 0;
		int rc = calc_eval(line, &result);

		if (rc != CALC_OK) {
			printf("calc: %s\n", calc_eval_strerror(rc));
			continue;
		}

		printf("%ld\n", result);
	}

	return 0;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		return interactive_mode();
	}

	if (argc == 2 && strcmp(argv[1], "-h") == 0) {
		printf("%s", HELP);
		return 0;
	}

	return argument_mode(argc, argv);
}
