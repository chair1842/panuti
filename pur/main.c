/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <panuti/stream.h>
#include <stdio.h>
#include <string.h>
#include <panuti/syscall/syscallsf.h>
#include <panuti/errno.h>
#include <panuti/process.h>
#include <panuti/handle.h>
#include <unistd.h>

#define PUR_PATH "/cd/usr/bin"

static int file_exists(const char* path) {
	int fd = handle_open(path);
	if (fd >= 0) {
		handle_close(fd);
		return 1;
	}
	
	if (fd == PANUTIERRNO_NOTFOUND) return 0;
	if (fd == PANUTIERRNO_UNSUPPORTEDOP) return 2;
	return -1;
}

static int shbt_help(int argc, char** argv) {
	printf("Available shell built-ins:\n");
	printf("  help - i mean, you're looking at this rn\n");
	printf("  exit - exit pur, it will come back anyways\n");
	printf("  cd - change the current working directory\n");

	return 0;
}

static int shbt_cd(int argc, char** argv) {
	if (argc < 2) {
		printf("pur: no path provided for cd\n");
		return -1;
	} else if (argc > 2) {
		printf("pur: why do you have more than one arg?\n");
	}
	
	int rc = panutisysf_chdir(argv[1]);
	switch (rc) {
		case PANUTIERRNO_NOTFOUND:
			printf("pur: path not found\n");
			return -1;
		case PANUTIERRNO_UNSUPPORTEDOP:
			printf("pur: path not a directory\n");
			return -1;
		case PANUTIERRNO_INVALIDADDR:
			printf("pur: what the chicken is this.\n");
			printf("by my pure coding skills, how does cd pass an invalid cowimpregnating pointer\n");
			return -1;
		default:
			return 0;
	}
}

static bool is_explicit_path(const char* s) {
	if (s[0] == '.') {
		return true;
	}
	
	for (const char* p = s; *p; p++) {
		if (*p == '/') {
			return true;
		}
	}
	
	return false;
}

static int run_external(const char* path, char** argv, int argc) {
	int fx = file_exists(path);
	switch (fx) {
		case 1:
			break;
		case 0:
			printf("pur: %s: command not found\n", argv[0]);
			return -1;
		case 2:
			printf("pur: %s: command provided is a directory\n", argv[0]);
			return -1;
		default:
			// we can't tell, continue.
			break;
	}
	
	pid_t pid = procreate(path, argv, argc, nullptr, 0, nullptr, 0);
	
	if (pid < 0) {
		if (pid == PANUTIERRNO_NOTFOUND) {
			// this shouldnt be reached
			printf("pur: %s: command not found\n", argv[0]);
		} else {
			printf("pur: %s: no thriving way my pointers are wrong\n", argv[0]);
		}
		
		return -1;
	}

	int exit_code;
	wait(pid, &exit_code);
	
	return exit_code;
}

static int tokenize(char* str, char** out_argv, int max_argv) {
	int argc = 0;
	char* p = str;

	while (*p) {
		// skip leading whitespace between tokens
		while (*p == ' ' || *p == '\t') {
			p++;
		}
		if (*p == '\0') {
			break;
		}

		if (argc >= max_argv) {
			return -1;
		}

		if (*p == '"') {
			p++; // skip opening quote
			out_argv[argc++] = p;

			while (*p != '"') {
				if (*p == '\0') {
					return -1; // unterminated quote
				}
				p++;
			}

			*p = '\0'; // terminate the token where the closing quote was
			p++;       // move past it
		} else {
			out_argv[argc++] = p;

			while (*p != ' ' && *p != '\t' && *p != '\0') {
				p++;
			}

			if (*p != '\0') {
				*p = '\0';
				p++;
			}
		}
	}

	return argc;
}

int input_command(int argc, char** argv) {
	if (argc == 0) {
		return 0;
	}

	if (is_explicit_path(argv[0])) {
		return run_external(argv[0], argv, argc);
	}

	// shell built-ins first
	if (strcmp(argv[0], "help") == 0) {
		return shbt_help(argc, argv);
	} else if (strcmp(argv[0], "exit") == 0) {
		panutisysf_exit(0);
	} else if (strcmp(argv[0], "cd") == 0) {
		return shbt_cd(argc, argv);
	}

	// fall back to PATH resolution
	char resolved[256];
	size_t path_len = strlen(PUR_PATH);
	size_t name_len = strlen(argv[0]);

	if (path_len + 1 + name_len + 1 > sizeof(resolved)) {
		printf("pur: %s: name too long\n", argv[0]);
		return -1;
	}

	size_t i = 0;
	for (size_t j = 0; j < path_len; j++) {
		resolved[i++] = PUR_PATH[j];
	}
	
	resolved[i++] = '/';
	for (size_t j = 0; j < name_len; j++) {
		resolved[i++] = argv[0][j];
	}
	
	resolved[i] = '\0';

	return run_external(resolved, argv, argc);
}

int main(int argc, char** argv) {
	while (1) {
		printf("# ");
		
		char buf[256];
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			switch (n) {
				case PANUTIERRNO_INVALIDADDR:
					printf("\npur: no way ts is happening, this cannot be real. buf is invalid\n");
					return -1;
				case PANUTIERRNO_BADFD:
					printf("\npur: on my balls i swear that 0 is in in_stream's range.\n");
					return -1;
				default:
					printf("\npur: did you forgot to update me you sunn of a female dog\n");
					return -1;
			}
		}
		
		buf[n] = '\0';

		char* cmd_argv[32];
		int cmd_argc = tokenize(buf, cmd_argv, 32);
		if (cmd_argc < 0) {
			printf("pur: the command provided has syntax errors\n");
			continue;
		}

		input_command(cmd_argc, cmd_argv);
	}
}