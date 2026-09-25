#include <panuti/stream.h>
#include <stdio.h>
#include <string.h>

int shbt_help(int argc, char** argv) {
	printf("Available shell built-ins:\n");
	printf("  help - i mean, you're looking at this rn.\n");

	return 0;
}

int tokenize(char* str, char** out_argv, int max_argv) {
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

	// shell built-ins first
	if (strcmp(argv[0], "help") == 0) {
		return shbt_help(argc, argv);
	} else {
		printf("pur: unrecognized shell built-in\n");
		return -1;
	}
}

int main(int argc, char** argv) {
	while (1) {
		printf("# ");
		
		char buf[256];
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("read from in0 failed\n");
			continue;
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