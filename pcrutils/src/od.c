/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

#define OD_ROW 16

enum {
	OD_HEX = 0,
	OD_OCT = 1,
	OD_CHAR = 2,
};

static int mode = OD_HEX;

static const char HEX[] = "0123456789abcdef";

static const char HELP[] =
	"od - a pcrutils utility\n\n"
	"od reads in0 to the end and writes a dump of its bytes to every out\n"
	"stream, sixteen to a row, with the byte offset in octal at the start\n"
	"of each row. the address column and the bytes stay readable whether\n"
	"the input is text or not\n\n"
	"usage:\n"
	"  od [-x] [-o] [-c]\n\n"
	"args:\n"
	"  -x - write the bytes in hex, the default\n"
	"  -o - write the bytes in octal\n"
	"  -c - write printable characters, and \\ooo for the rest\n"
	"  -h - prints this help message\n\n"
	"od reads a stream, so it takes no paths, try in a > od\n"
	"the whole input has to fit in memory, the way split does\n";

static void bytes_hex(pcr_buf_t* b, const unsigned char* p, size_t n) {
	for (size_t i = 0; i < n; i++) {
		pcr_buf_append(b, &HEX[p[i] >> 4], 1);
		pcr_buf_append(b, &HEX[p[i] & 0xF], 1);
		pcr_buf_append(b, " ", 1);
	}
}

static void bytes_oct(pcr_buf_t* b, const unsigned char* p, size_t n) {
	for (size_t i = 0; i < n; i++) {
		char o[4];
		o[0] = (char)('0' + ((p[i] >> 6) & 7));
		o[1] = (char)('0' + ((p[i] >> 3) & 7));
		o[2] = (char)('0' + (p[i] & 7));
		o[3] = ' ';
		pcr_buf_append(b, o, 4);
	}
}

static void bytes_char(pcr_buf_t* b, const unsigned char* p, size_t n) {
	for (size_t i = 0; i < n; i++) {
		unsigned char c = p[i];

		if (c >= 0x20 && c <= 0x7E && c != '\\' && c != '\'') {
			char q[4] = {'\'', (char)c, '\'', ' '};
			pcr_buf_append(b, q, 4);
		} else if (c == '\\' || c == '\'') {
			char q[4] = {'\\', (char)c, ' ', 0};
			q[2] = ' ';
			pcr_buf_append(b, q, 3);
		} else {
			char o[5];
			o[0] = '\\';
			o[1] = (char)('0' + ((c >> 6) & 7));
			o[2] = (char)('0' + ((c >> 3) & 7));
			o[3] = (char)('0' + (c & 7));
			o[4] = ' ';
			pcr_buf_append(b, o, 5);
		}
	}
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			printf("pcrutils: od: unexpected argument '%s'\n", a);
			printf("od reads in stream 0, it takes no paths\n");
			return -1;
		}

		if (strcmp(a, "-x") == 0) {
			mode = OD_HEX;
		} else if (strcmp(a, "-o") == 0) {
			mode = OD_OCT;
		} else if (strcmp(a, "-c") == 0) {
			mode = OD_CHAR;
		} else {
			printf("pcrutils: od: invalid option '%s'\n", a + 1);
			return -1;
		}
	}

	int counts[2] = {0};
	nstream(counts);

	if (counts[0] < 1) {
		printf("pcrutils: od: there is no in stream to read\n");
		return -1;
	}

	if (counts[1] < 1) {
		printf("pcrutils: od: there are no out streams to write to\n");
		return -1;
	}

	pcr_buf_t in = {0};
	char buf[128];

	for (;;) {
		int n = stream_read(0, buf, sizeof(buf));
		if (n < 0) {
			printf("pcrutils: od: could not read the in stream\n");
			pcr_buf_free(&in);
			return -1;
		}

		if (n == 0) {
			break;
		}

		if (pcr_buf_append(&in, buf, (size_t)n) != 0) {
			printf("pcrutils: od: out of memory\n");
			pcr_buf_free(&in);
			return -1;
		}
	}

	pcr_buf_t out = {0};

	for (size_t off = 0; off < in.len; off += OD_ROW) {
		char addr[16];
		int an = pcr_uformat((unsigned long)off, 8, addr);

		for (int pad = an; pad < 7; pad++) {
			pcr_buf_append(&out, " ", 1);
		}
		pcr_buf_append(&out, addr, (size_t)an);
		pcr_buf_append(&out, " ", 1);

		size_t n = in.len - off;
		if (n > OD_ROW) {
			n = OD_ROW;
		}

		const unsigned char* p = (const unsigned char*)in.data + off;

		if (mode == OD_HEX) {
			bytes_hex(&out, p, n);
		} else if (mode == OD_OCT) {
			bytes_oct(&out, p, n);
		} else {
			bytes_char(&out, p, n);
		}

		pcr_buf_append(&out, "\n", 1);
	}

	int failed = 0;

	if (out.len > 0 && pcr_output(counts[1], out.data, out.len, 0) != 0) {
		printf("pcrutils: od: could not write to the out streams\n");
		failed = 1;
	}

	pcr_buf_free(&in);
	pcr_buf_free(&out);
	return failed ? -1 : 0;
}