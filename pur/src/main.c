/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ast.h"
#include "exec.h"
#include "parse.h"
#include "run.h"

#include <panuti/errno.h>
#include <panuti/handle.h>
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

// parse and run one mutable line. empty lines are fine, errors are reported
// here so both the prompt and a script share the same behaviour.
static int run_line(char* text, int* status_out) {
	pur_ast_reset();

	int rc = pur_parse_command(text);
	if (rc == PUR_PARSE_EMPTY) {
		return 0;
	}

	if (rc != PUR_PARSE_OK) {
		printf("pur: %s\n", pur_parse_strerror(rc));
		return -1;
	}

	int status = 0;
	if (pur_run_command(false, &status) != 0) {
		return -1;
	}

	if (status_out) {
		*status_out = status;
	}

	return 0;
}

// blank lines and lines whose first non-space character is '#' are comments
static bool skip_line(const char* s) {
	while (*s == ' ' || *s == '\t' || *s == '\r') {
		s++;
	}

	return *s == '\0' || *s == '#';
}

// read a script a chunk at a time and run each newline-terminated line. the
// line buffer is shared with the prompt, but a script returns before the
// prompt is reached, so the two never overlap.
static int run_script(const char* path) {
	int fd = handle_open(path);
	if (fd < 0) {
		printf("pur: could not open script '%s'\n", path);
		return 1;
	}

	if (line_reserve(PUR_LINE_INIT) != 0) {
		handle_close(fd);
		printf("pur: out of memory\n");
		return 1;
	}

	char chunk[256];
	size_t used = 0;
	int status = 0;
	int rc = 0;

	while (1) {
		int n = handle_read(fd, chunk, sizeof(chunk));
		if (n < 0) {
			printf("pur: could not read script '%s'\n", path);
			rc = 1;
			break;
		}

		if (n == 0) {
			break;
		}

		for (int i = 0; i < n; i++) {
			if (chunk[i] == '\n') {
				line[used] = '\0';
				used = 0;

				if (!skip_line(line)) {
					int s = 0;
					if (run_line(line, &s) == 0) {
						status = s;
					}
				}

				continue;
			}

			if (used + 1 >= line_cap) {
				if (line_reserve(line_cap * 2) != 0) {
					printf("pur: out of memory\n");
					rc = 1;
					break;
				}
			}

			line[used++] = chunk[i];
		}

		if (rc != 0) {
			break;
		}
	}

	// a final line with no newline is still a line
	if (rc == 0 && used > 0) {
		line[used] = '\0';
		if (!skip_line(line)) {
			int s = 0;
			if (run_line(line, &s) == 0) {
				status = s;
			}
		}
	}

	handle_close(fd);
	return rc != 0 ? rc : (status & 0xff);
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

			int prc = pur_parse_command(stmt_buf);
			if (prc != PUR_PARSE_OK) {
				free(stmt_buf);
				printf("pur: %s\n", pur_parse_strerror(prc));
				return 1;
			}

			int status = 0;
			int drc = pur_run_command(dry_run, &status);
			free(stmt_buf);
			if (drc != 0) {
				return 1;
			}

			// exit statuses are 8 bits, and a child that returned -1 comes
			// back as 0xffffffff, so bring it into range
			return dry_run ? 0 : (int)(status & 0xff);
		}

		// a non-flag argument names a script: run its lines and exit
		if (argv[1][0] != '-') {
			return run_script(argv[1]);
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

		run_line(line, NULL);

		putchar(0x1B);
		putchar('R');
	}
}
