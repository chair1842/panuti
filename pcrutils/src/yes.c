/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/stream.h>
#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"yes - a pcrutils utility\n\n"
	"yes prints its word, or y when none is given, to every out stream\n"
	"over and over until the stream stops taking it\n\n"
	"usage:\n"
	"  yes [word ...]\n\n"
	"args:\n"
	"  -h - prints this help message\n";

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	char word[512];
	size_t wlen = 0;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (wlen > 0) {
			word[wlen++] = ' ';
		}

		if (wlen + strlen(a) + 1 > sizeof(word)) {
			printf("pcrutils: yes: the word is too long\n");
			return -1;
		}

		memcpy(word + wlen, a, strlen(a));
		wlen += strlen(a);
	}

	if (wlen == 0) {
		word[wlen++] = 'y';
	}

	word[wlen] = '\n';

	int counts[2] = {0};
	nstream(counts);

	if (counts[1] < 1) {
		printf("pcrutils: yes: there are no out streams to write to\n");
		return -1;
	}

	// the stream is full or closed when pcr_output starts failing, and that
	// is the natural way for yes to come to an end
	while (pcr_output(counts[1], word, wlen + 1, 0) == 0) {
	}

	return 0;
}