/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <panuti/handle.h>
#include <panuti/stream.h>
#include <panuti/syscall/syscallsf.h>

#include <pcrutils/pcrutils.h>

// read the kernel clock. 0 on success, -1 on failure; sets centis
int pcr_uptime_centis(long* out) {
	int32_t fd = handle_open("/dvc/uptime");
	if (fd < 0) {
		return -1;
	}

	char buf[32];
	int32_t n = handle_read(fd, buf, sizeof(buf) - 1);
	handle_close(fd);
	if (n < 0) {
		return -1;
	}

	long centis = 0;
	for (int i = 0; i < n && buf[i] >= '0' && buf[i] <= '9'; i++) {
		if (centis > (LONG_MAX - (long)(buf[i] - '0')) / 10) {
			return -1;
		}
		centis = centis * 10 + (long)(buf[i] - '0');
	}

	*out = centis;
	return 0;
}

int pcr_write_all(int stream, const char* buf, size_t len) {
	size_t off = 0;

	while (off < len) {
		int w = stream_write(stream, buf + off, len - off);
		if (w <= 0) {
			return -1;
		}

		off += (size_t)w;
	}

	return 0;
}

// the handle equivalent of pcr_write_all: a file write can stop short
int pcr_handle_write_all(int fd, const char* buf, size_t len) {
	size_t off = 0;

	while (off < len) {
		int w = handle_write(fd, buf + off, len - off);
		if (w <= 0) {
			return -1;
		}

		off += (size_t)w;
	}

	return 0;
}

// move a handle's write position to the end of the file, the only way to
// append without a seek. reading pulls the offset along, and a device that
// cannot be read is left alone
int pcr_handle_to_end(int fd) {
	char buf[128];

	for (;;) {
		int n = handle_read(fd, buf, sizeof(buf));
		if (n <= 0) {
			return n;
		}
	}
}

// open a path, creating the file first if it is not there yet. mkfile is
// a "create if absent" by nature, and when it fails the open decides whether
// the path really exists. returns an fd or a negative error
int pcr_open_or_create(const char* path) {
	if (panutisysf_mkfile(path) < 0) {
		// the name may simply already be there; let open say what is what
	}

	return handle_open(path);
}

// this libc declares strchr but never builds it, so the utils get one here.
// memchr does the work, the helper is just what a caller would write with
// strchr
const char* pcr_strchr(const char* s, char c) {
	size_t n = strlen(s) + 1;
	return memchr(s, c, n);
}

int pcr_output(int no_streams, const char* buf, size_t len, int only_out0) {
	int streams = only_out0 ? 1 : no_streams;
	int failed = 0;

	for (int s = 0; s < streams; s++) {
		if (pcr_write_all(s, buf, len) != 0) {
			failed = 1;
		}
	}

	return failed ? -1 : 0;
}

bool pcr_help_wanted(int argc, char** argv, const char* help) {
	if (!pcr_option_provided(argc, argv, "h")) {
		return false;
	}

	printf("%s", help);

	return true;
}

bool pcr_option_provided(int argc, char** argv, const char* option) {
	const char* opt = (option[0] == '-') ? option + 1 : option;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			continue;
		}

		if (strcmp(a + 1, opt) == 0) {
			return true;
		}
	}

	return false;
}

#define PCR_BUF_INITIAL 128

static int pcr_buf_reserve(pcr_buf_t* b, size_t need) {
	if (need <= b->cap) {
		return 0;
	}

	size_t want = b->cap ? b->cap : PCR_BUF_INITIAL;

	while (want < need) {
		if (want > (size_t)-1 / 2) {
			return -1;
		}
		want *= 2;
	}

	char* p = realloc(b->data, want);
	if (!p) {
		return -1;
	}

	b->data = p;
	b->cap = want;
	return 0;
}

int pcr_buf_append(pcr_buf_t* b, const char* data, size_t n) {
	if (pcr_buf_reserve(b, b->len + n) != 0) {
		return -1;
	}

	memcpy(b->data + b->len, data, n);
	b->len += n;
	return 0;
}

void pcr_buf_free(pcr_buf_t* b) {
	free(b->data);
	*b = (pcr_buf_t){0};
}

#define PCR_LINES_INITIAL 16

static int pcr_lines_grow(pcr_lines_t* lines) {
	if (lines->n < lines->cap) {
		return 0;
	}

	size_t want = lines->cap ? lines->cap * 2 : PCR_LINES_INITIAL;
	char** p = realloc(lines->v, want * sizeof(char*));
	if (!p) {
		return -1;
	}

	lines->v = p;
	lines->cap = want;
	return 0;
}

static int pcr_lines_append(pcr_lines_t* lines, char* line) {
	if (pcr_lines_grow(lines) != 0) {
		return -1;
	}

	lines->v[lines->n++] = line;
	return 0;
}

void pcr_lines_free(pcr_lines_t* lines) {
	for (size_t i = 0; i < lines->n; i++) {
		free(lines->v[i]);
	}
	free(lines->v);
	*lines = (pcr_lines_t){0};
}

int pcr_stream_lines(int stream, pcr_lines_t* lines) {
	pcr_buf_t b = {0};
	*lines = (pcr_lines_t){0};

	char buf[128];

	for (;;) {
		int n = stream_read(stream, buf, sizeof(buf));
		if (n < 0) {
			goto fail;
		}

		if (n == 0) {
			break;
		}

		if (pcr_buf_append(&b, buf, (size_t)n) != 0) {
			goto fail;
		}
	}

	size_t start = 0;

	for (size_t i = 0; i < b.len; i++) {
		if (b.data[i] == '\n') {
			size_t line_len = i - start + 1;
			char* line = malloc(line_len + 1);
			if (!line) {
				goto fail;
			}

			memcpy(line, b.data + start, line_len);
			line[line_len] = '\0';

			if (pcr_lines_append(lines, line) != 0) {
				free(line);
				goto fail;
			}

			start = i + 1;
		}
	}

	// a last line with no newline of its own is still a line
	if (start < b.len) {
		size_t line_len = b.len - start;
		char* line = malloc(line_len + 1);
		if (!line) {
			goto fail;
		}

		memcpy(line, b.data + start, line_len);
		line[line_len] = '\0';

		if (pcr_lines_append(lines, line) != 0) {
			free(line);
			goto fail;
		}
	}

	pcr_buf_free(&b);
	return 0;

fail:
	pcr_buf_free(&b);
	pcr_lines_free(lines);
	return -1;
}

typedef struct {
	bool reverse;
	bool numeric;
} pcr_sort_ctx_t;

// the leading integer of a string, as sort -n would read it, or -1 if the
// string does not open with a number. a run of digits after an optional sign.
static int pcr_numeric_prefix(const char* s, long* out) {
	long v = 0;
	bool neg = false;

	while (*s == ' ') {
		s++;
	}

	if (*s == '-') {
		neg = true;
		s++;
	}

	if (*s < '0' || *s > '9') {
		return -1;
	}

	while (*s >= '0' && *s <= '9') {
		if (v > (LONG_MAX - 9) / 10) {
			return -1;
		}
		v = v * 10 + (*s - '0');
		s++;
	}

	*out = neg ? -v : v;
	return 0;
}

static int pcr_str_cmp(const pcr_sort_ctx_t* ctx, const char* a, const char* b) {
	int c;

	if (ctx->numeric) {
		long av, bv;
		int an = pcr_numeric_prefix(a, &av);
		int bn = pcr_numeric_prefix(b, &bv);

		if (an == 0 && bn == 0) {
			if (av < bv) {
				c = -1;
			} else if (av > bv) {
				c = 1;
			} else {
				c = strcmp(a, b);
			}
		} else {
			c = strcmp(a, b);
		}
	} else {
		c = strcmp(a, b);
	}

	return ctx->reverse ? -c : c;
}

static void pcr_strs_qsort(char** v, size_t n, const pcr_sort_ctx_t* ctx) {
	if (n <= 1) {
		return;
	}

	size_t pivot = n - 1;
	size_t i = 0;

	for (size_t j = 0; j < pivot; j++) {
		if (pcr_str_cmp(ctx, v[j], v[pivot]) < 0) {
			char* t = v[i];
			v[i] = v[j];
			v[j] = t;
			i++;
		}
	}

	char* t = v[i];
	v[i] = v[pivot];
	v[pivot] = t;

	if (i > 0) {
		pcr_strs_qsort(v, i, ctx);
	}
	if (i + 1 < n) {
		pcr_strs_qsort(v + i + 1, n - i - 1, ctx);
	}
}

void pcr_strs_sort(char** v, size_t n, bool reverse, bool numeric) {
	pcr_sort_ctx_t ctx = {reverse, numeric};
	pcr_strs_qsort(v, n, &ctx);
}

long pcr_parse_long(const char* s, bool* ok) {
	long v = 0;

	if (*s == '\0') {
		*ok = false;
		return 0;
	}

	for (const char* p = s; *p; p++) {
		if (*p < '0' || *p > '9') {
			*ok = false;
			return 0;
		}

		if (v > (LONG_MAX - (*p - '0')) / 10) {
			*ok = false;
			return 0;
		}

		v = v * 10 + (*p - '0');
	}

	*ok = true;
	return v;
}

int pcr_uformat(unsigned long v, int base, char* out) {
	static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

	if (base < 2 || base > 36) {
		return 0;
	}

	char tmp[33];
	int n = 0;

	if (v == 0) {
		tmp[n++] = '0';
	}

	while (v > 0) {
		tmp[n++] = digits[v % (unsigned long)base];
		v /= (unsigned long)base;
	}

	for (int i = 0; i < n; i++) {
		out[i] = tmp[n - 1 - i];
	}
	out[n] = '\0';

	return n;
}

// match an '[' group, pat pointing at the '['. advances both on a match.
// returns false for an unterminated or empty group, so the caller can treat
// the '[' as a literal character instead.
static bool pcr_glob_bracket(const char** pat_in, const char** s_in) {
	const char* pat = *pat_in;
	const char* s = *s_in;
	unsigned char ch = (unsigned char)*s;

	if (ch == '\0') {
		return false;
	}

	pat++;

	bool neg = false;
	if (*pat == '!' || *pat == '^') {
		neg = true;
		pat++;
	}

	bool matched = false;
	bool any = false;

	while (*pat && *pat != ']') {
		any = true;
		char lo = *pat;

		if (pat[1] == '-' && pat[2] && pat[2] != ']') {
			char hi = pat[2];
			if (ch >= (unsigned char)lo && ch <= (unsigned char)hi) {
				matched = true;
			}
			pat += 3;
		} else {
			if (ch == (unsigned char)lo) {
				matched = true;
			}
			pat++;
		}
	}

	if (!any || *pat != ']') {
		return false;
	}

	*pat_in = pat + 1;
	*s_in = s + 1;
	return neg ? !matched : matched;
}

bool pcr_glob_match(const char* pat, const char* s) {
	const char* star = NULL;
	const char* ss = s;

	while (*s) {
		if (*pat == '*') {
			star = pat++;
			ss = s;
			continue;
		}

		if (*pat == '?') {
			pat++;
			s++;
			continue;
		}

		if (*pat == '[') {
			const char* p = pat;
			const char* cp = s;

			if (pcr_glob_bracket(&p, &cp)) {
				pat = p;
				s = cp;
				continue;
			}

			if (*pat == *s) {
				pat++;
				s++;
				continue;
			}

			if (star) {
				pat = star + 1;
				s = ++ss;
				continue;
			}

			return false;
		}

		if (*pat == *s) {
			pat++;
			s++;
			continue;
		}

		if (star) {
			pat = star + 1;
			s = ++ss;
			continue;
		}

		return false;
	}

	while (*pat == '*') {
		pat++;
	}

	return *pat == '\0';
}

char* pcr_path_join(const char* base, const char* name) {
	size_t bl = strlen(base);
	size_t nl = strlen(name);

	if (bl == 0) {
		char* out = malloc(nl + 1);
		if (!out) {
			return NULL;
		}

		memcpy(out, name, nl + 1);
		return out;
	}

	bool slash = base[bl - 1] == '/';
	size_t need = bl + nl + 1 + (slash ? 0 : 1);

	char* out = malloc(need);
	if (!out) {
		return NULL;
	}

	memcpy(out, base, bl);
	size_t i = bl;

	if (!slash) {
		out[i++] = '/';
	}

	memcpy(out + i, name, nl + 1);
	return out;
}