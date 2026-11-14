/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>

#include <panuti/handle.h>

#include <pcrutils/pcrutils.h>

#define PAGE_KIB 4UL
#define KIB_PER_MIB 1024UL
#define KIB_PER_GIB (1024UL * 1024UL)

static const char HELP[] =
	"free - a pcrutils utility\n"
	"free prints the reported memory info of the system\n\n"
	"args:\n"
	"  -h - prints this help message\n"
	"  -k - use KiB only\n";

static int read_pages(const char* path, unsigned long* out) {
	char buf[32];

	int fd = handle_open(path);
	if (fd < 0) {
		printf("pcrutils: free: could not open %s\n", path);
		return -1;
	}

	int n = handle_read(fd, buf, sizeof(buf) - 1);
	handle_close(fd);
	if (n <= 0) {
		printf("pcrutils: free: could not read %s\n", path);
		return -1;
	}

	buf[n] = '\0';
	while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == ' ')) {
		buf[--n] = '\0';
	}

	bool ok;
	long v = pcr_parse_long(buf, &ok);
	if (!ok) {
		printf("pcrutils: free: %s is not a number\n", path);
		return -1;
	}

	*out = (unsigned long)v;
	return 0;
}

static void print_size(const char* label, unsigned long kib, bool kib_only) {
	if (kib_only) {
		printf("%s: %luKiB\n", label, kib);
		return;
	}

	const char* unit;
	unsigned long div;

	if (kib >= KIB_PER_GIB) {
		unit = "GiB";
		div = KIB_PER_GIB;
	} else if (kib >= KIB_PER_MIB) {
		unit = "MiB";
		div = KIB_PER_MIB;
	} else {
		unit = "KiB";
		div = 1;
	}

	unsigned long whole = kib / div;
	unsigned long tenths = (kib % div) * 10 / div;

	if (tenths == 0) {
		printf("%s: %lu%s\n", label, whole, unit);
	} else {
		printf("%s: %lu.%lu%s\n", label, whole, tenths, unit);
	}
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	bool kib_only = pcr_option_provided(argc, argv, "k");

	unsigned long total;
	unsigned long free_pages;

	if (read_pages("/sys/meminfo/total", &total) < 0 || read_pages("/sys/meminfo/free", &free_pages) < 0) {
		return -1;
	}

	unsigned long used = total >= free_pages ? total - free_pages : 0;

	print_size("Used", used * PAGE_KIB, kib_only);
	print_size("Free", free_pages * PAGE_KIB, kib_only);
	print_size("Total", total * PAGE_KIB, kib_only);

	return 0;
}