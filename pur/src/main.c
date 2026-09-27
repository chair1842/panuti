/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ast.h"
#include "exec.h"
#include "parse.h"

#include <panuti/errno.h>
#include <panuti/stream.h>
#include <panuti/syscall/syscallsf.h>

#include <stdio.h>
#include <string.h>

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

			// static: pur only has 4 pages of user stack to give away
			static char stmt_buf[PUR_LINE_MAX];

			if (join_statement(argc, argv, stmt_buf, sizeof(stmt_buf)) != 0) {
				printf("pur: statement is too long\n");
				return 1;
			}

			pur_ast_reset();

			int prc = pur_parse(stmt_buf);
			if (prc != PUR_PARSE_OK) {
				printf("pur: %s\n", pur_parse_strerror(prc));
				return 1;
			}

			int status = 0;
			int xrc = dry_run ? pur_exec_plan() : pur_exec(&status);
			if (xrc != PUR_EXEC_OK) {
				printf("pur: %s\n", pur_exec_strerror(xrc));
				return 1;
			}

			// exit statuses are 8 bits, and a child that returned -1 comes
			// back as 0xffffffff, so bring it into range
			return dry_run ? 0 : (int)(status & 0xff);
		}
	}

	char buf[PUR_LINE_MAX];

	while (1) {
		printf("# ");

		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("\npur: could not read the input stream\n");
			continue;
		}

		if (n >= (int)sizeof(buf)) {
			n = (int)sizeof(buf) - 1;
		}

		buf[n] = '\0';

		pur_ast_reset();

		int rc = pur_parse(buf);
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
