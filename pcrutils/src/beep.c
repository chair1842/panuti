/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <panuti/handle.h>
#include <panuti/process.h>
#include <pcrutils/pcrutils.h>

#define BEEP_DEFAULT_FREQ 1000
#define BEEP_DEFAULT_CS 20
#define BEEP_MIN_FREQ 19
#define BEEP_MAX_FREQ 1193182

static const char HELP[] =
	"beep - a pcrutils utility\n\n"
	"beep sounds the PC speaker through /dvc/beepr\n\n"
	"usage:\n"
	"  beep\n"
	"  beep <frequency>\n"
	"  beep <frequency> <centiseconds>\n"
	"  beep [-f <frequency>] [-d <centiseconds>]\n\n"
	"args:\n"
	"  -f <hz> - tone frequency, default 1000\n"
	"  -d <cs> - how long to sound, in centiseconds, default 20\n"
	"  -h - prints this help message\n\n"
	"the tone stops when the handle is closed, so beep keeps /dvc/beepr\n"
	"open until the time is up\n\n"
	"examples:\n"
	"  beep\n"
	"  beep 880\n"
	"  beep 440 50\n";

static int parse_number(const char* s, long* out) {
	bool ok;
	*out = pcr_parse_long(s, &ok);

	if (!ok) {
		return -1;
	}

	return 0;
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	long frequency = BEEP_DEFAULT_FREQ;
	long centis = BEEP_DEFAULT_CS;
	int positional = 0;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (strcmp(a, "-f") == 0 || strcmp(a, "-d") == 0) {
			if (i + 1 >= argc) {
				printf("pcrutils: beep: %s needs a value\n", a);
				return -1;
			}

			long value;
			if (parse_number(argv[++i], &value) != 0) {
				printf("pcrutils: beep: '%s' is not a number\n", argv[i]);
				return -1;
			}

			if (a[1] == 'f') {
				frequency = value;
			} else {
				centis = value;
			}

			continue;
		}

		if (a[0] == '-' && a[1] != '\0') {
			printf("pcrutils: beep: invalid option '%s'\n", a + 1);
			return -1;
		}

		long value;
		if (parse_number(a, &value) != 0) {
			printf("pcrutils: beep: '%s' is not a number\n", a);
			return -1;
		}

		if (positional == 0) {
			frequency = value;
		} else if (positional == 1) {
			centis = value;
		} else {
			printf("pcrutils: beep: too many arguments\n");
			return -1;
		}

		positional++;
	}

	if (frequency < BEEP_MIN_FREQ || frequency > BEEP_MAX_FREQ) {
		printf("pcrutils: beep: frequency must be between %d and %d Hz\n",
			BEEP_MIN_FREQ, BEEP_MAX_FREQ);
		return -1;
	}

	if (centis > INT_MAX) {
		printf("pcrutils: beep: the duration is too long\n");
		return -1;
	}

	char buf[16];
	int len = pcr_uformat((unsigned long)frequency, 10, buf);
	buf[len++] = '\n';

	int fd = handle_open("/dvc/beepr");
	if (fd < 0) {
		printf("pcrutils: beep: could not open /dvc/beepr\n");
		return -1;
	}

	int written = handle_write(fd, buf, (size_t)len);
	if (written != len) {
		handle_close(fd);

		if (written < 0) {
			printf("pcrutils: beep: /dvc/beepr rejected the frequency\n");
		} else {
			printf("pcrutils: beep: could not write to /dvc/beepr\n");
		}

		return -1;
	}

	if (centis > 0 && sleep((int)centis) < 0) {
		handle_close(fd);
		printf("pcrutils: beep: could not sleep\n");
		return -1;
	}

	handle_close(fd);
	return 0;
}
