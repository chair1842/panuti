/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"printf - a pcrutils utility\n\n"
	"printf formats a string, writing it out to every out stream\n\n"
	"usage:\n"
	"  printf format [arg ...]\n\n"
	"the format holds %s (a string), %d %i (a number), %c (a single\n"
	"character), %x (a number in hex) and %%, and the escapes \\n \\t \\r\n"
	"\\v \\f \\a \\b and \\\\\n\n"
	"args:\n"
	"  -h - prints this help message\n\n"
	"a %d, %i or %x takes its number from the next argument\n";

static bool parse_signed(const char* s, long* out) {
	bool neg = false;

	if (*s == '-') {
		neg = true;
		s++;
	}

	bool ok;
	long v = pcr_parse_long(s, &ok);
	if (!ok) {
		return false;
	}

	*out = neg ? -v : v;
	return true;
}

// a negative number needs the '-' put back on by hand, there is no %ld here
static int format_signed(long v, char* out) {
	if (v < 0) {
		out[0] = '-';
		int n = pcr_uformat((unsigned long)(-(v + 1)) + 1UL, 10, out + 1);
		return n + 1;
	}

	return pcr_uformat((unsigned long)v, 10, out);
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	if (argc < 2) {
		printf("pcrutils: printf: no format given\n");
		return -1;
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[1] < 1) {
		printf("pcrutils: printf: there are no out streams to write to\n");
		return -1;
	}

	const char* fmt = argv[1];
	int argi = 2;

	pcr_buf_t out = {0};
	int failed = 0;

	for (const char* p = fmt; *p; p++) {
		char c = *p;

		if (c == '\\') {
			char esc = p[1];

			if (p[1] == '\0') {
				if (pcr_buf_append(&out, "\\", 1) != 0) {
					goto oom;
				}
				break;
			}

			char mapped;
			switch (esc) {
				case 'n': mapped = '\n'; break;
				case 't': mapped = '\t'; break;
				case 'r': mapped = '\r'; break;
				case 'v': mapped = '\v'; break;
				case 'f': mapped = '\f'; break;
				case 'a': mapped = '\a'; break;
				case 'b': mapped = '\b'; break;
				case '\\': mapped = '\\'; break;
				default: mapped = esc; break;
			}

			if (pcr_buf_append(&out, &mapped, 1) != 0) {
				goto oom;
			}
			p++;
			continue;
		}

		if (c != '%') {
			if (pcr_buf_append(&out, &c, 1) != 0) {
				goto oom;
			}
			continue;
		}

		char conv = p[1];

		if (conv == '\0') {
			failed = 1;
			printf("pcrutils: printf: a dangling %% at the end of the format\n");
			break;
		}

		if (conv == '%') {
			if (pcr_buf_append(&out, "%", 1) != 0) {
				goto oom;
			}
			p++;
			continue;
		}

		if (argi >= argc) {
			failed = 1;
			printf("pcrutils: printf: not enough arguments for %%%c\n", conv);
			break;
		}

		const char* arg = argv[argi++];

		if (conv == 's') {
			if (pcr_buf_append(&out, arg, strlen(arg)) != 0) {
				goto oom;
			}
		} else if (conv == 'c') {
			if (pcr_buf_append(&out, arg, 1) != 0) {
				goto oom;
			}
		} else if (conv == 'd' || conv == 'i') {
			long v;
			if (!parse_signed(arg, &v)) {
				failed = 1;
				printf("pcrutils: printf: '%s' is not a number\n", arg);
				break;
			}
			char num[33];
			int n = format_signed(v, num);
			if (pcr_buf_append(&out, num, (size_t)n) != 0) {
				goto oom;
			}
		} else if (conv == 'x' || conv == 'X') {
			bool ok;
			long v = pcr_parse_long(arg, &ok);
			if (!ok) {
				failed = 1;
				printf("pcrutils: printf: '%s' is not a number\n", arg);
				break;
			}
			char num[33];
			int n = pcr_uformat((unsigned long)v, 16, num);
			if (pcr_buf_append(&out, num, (size_t)n) != 0) {
				goto oom;
			}
		} else {
			failed = 1;
			printf("pcrutils: printf: no %%%c here, only s d i c x %% and the escapes\n", conv);
			break;
		}

		p++;
		continue;

	oom:
		failed = 1;
		break;
	}

	if (!failed && out.len > 0) {
		if (pcr_output(counts[1], out.data, out.len, 0) != 0) {
			printf("pcrutils: printf: could not write to the out streams\n");
			failed = 1;
		}
	}

	pcr_buf_free(&out);
	return failed ? -1 : 0;
}