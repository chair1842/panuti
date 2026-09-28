/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ast.h"
#include "exec.h"
#include "parse.h"

#include <panuti/errno.h>
#include <panuti/stream.h>
#include <panuti/syscall/syscallsf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the line the shell reads into starts this big and grows with realloc. pur
// only has 4 pages of user stack to give away, so nothing that can be sized
// by what came in belongs in a frame.
static char* line;
static size_t line_cap;

static int line_reserve(size_t need) {
	if (need <= line_cap) {
		return 0;
	}

	size_t want = line_cap ? line_cap : PUR_LINE_INIT;

	while (want < need) {
		if (want > (size_t)-1 / 2) {
			return -1;
		}
		want *= 2;
	}

	char* p = realloc(line, want);
	if (!p) {
		return -1;
	}

	line = p;
	line_cap = want;
	return 0;
}

// -n and -c both take the statement as the rest of the command line, so the
// arguments get joined back into the single string the parser wants
static int join_statement(int argc, char** argv, char* buf, size_t cap) {
	char* p = buf;

	for (int i = 2; i < argc; i++) {
		size_t len = strlen(argv[i]);

		if (i > 2) {
			if ((size_t)(p - buf) + 1 >= cap) {
				return -1;
			}

			*p++ = ' ';
		}

		if ((size_t)(p - buf) + len + 1 > cap) {
			return -1;
		}

		for (size_t j = 0; j < len; j++) {
			*p++ = argv[i][j];
		}
	}

	*p = '\0';
	return 0;
}

int main(int argc, char** argv) {
	if (argc > 1) {
		if (strcmp(argv[1], "-v") == 0) {
			printf("pur 0.2 - the panuti shell\n");
			return 0;
		}

		if (strcmp(argv[1], "-h") == 0) {
			return pur_builtin_help();
		}

		// -n <statement>: wire it, report stream counts, run nothing
		// -c <statement>: run it, then exit with the pipeline's status
		if (argc > 2 && (strcmp(argv[1], "-n") == 0 || strcmp(argv[1], "-c") == 0)) {
			bool dry_run = argv[1][1] == 'n';

			// the statement goes on the heap rather than in a static, so a
			// long one is not held to a size the machine cannot promise
			size_t need = 1;

			for (int i = 2; i < argc; i++) {
				need += strlen(argv[i]) + 1;
			}

			char* stmt_buf = malloc(need);
			if (!stmt_buf) {
				printf("pur: out of memory\n");
				return 1;
			}

			int jrc = join_statement(argc, argv, stmt_buf, need);
			if (jrc != 0) {
				free(stmt_buf);
				printf("pur: statement is too long\n");
				return 1;
			}

			pur_ast_reset();

			int prc = pur_parse(stmt_buf);
			if (prc != PUR_PARSE_OK) {
				free(stmt_buf);
				printf("pur: %s\n", pur_parse_strerror(prc));
				return 1;
			}

			int status = 0;
			int xrc = dry_run ? pur_exec_plan() : pur_exec(&status);
			free(stmt_buf);
			if (xrc != PUR_EXEC_OK) {
				printf("pur: %s\n", pur_exec_strerror(xrc));
				return 1;
			}

			// exit statuses are 8 bits, and a child that returned -1 comes
			// back as 0xffffffff, so bring it into range
			return dry_run ? 0 : (int)(status & 0xff);
		}
	}

	while (1) {
		printf("# ");

		if (line_reserve(PUR_LINE_INIT) != 0) {
			printf("\npur: out of memory\n");
			return 1;
		}

		int n = stream_read(0, line, line_cap);
		if (n < 0) {
			printf("\npur: could not read the input stream\n");
			continue;
		}

		if ((size_t)n == line_cap) {
			// the buffer filled up, so the line was cut short. grow it so
			// the next one has more room. the line stays cut here, as it
			// always was: stdin gives a line at a time, so another read
			// would bring back the line after this one
			if (line_reserve(line_cap * 2) != 0) {
				printf("\npur: out of memory\n");
				return 1;
			}
		}

		if (n >= (int)line_cap) {
			n = (int)line_cap - 1;
		}

		line[n] = '\0';

		pur_ast_reset();

		int rc = pur_parse(line);
		if (rc == PUR_PARSE_EMPTY) {
			continue;
		}

		if (rc != PUR_PARSE_OK) {
			printf("pur: %s\n", pur_parse_strerror(rc));
			continue;
		}

		int status = 0;
		rc = pur_exec(&status);
		if (rc != PUR_EXEC_OK) {
			printf("pur: %s\n", pur_exec_strerror(rc));
		}
	}
}
