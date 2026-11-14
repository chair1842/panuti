/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>

#include <panuti/dirent.h>
#include <panuti/handle.h>
#include <panuti/inode_type.h>
#include <panuti/stat.h>

#include <pcrutils/pcrutils.h>

#define PS_PID_DIR "/sys/pid"
#define PS_MAX_TASKS 128
#define PS_CMD_WIDTH 40
#define PS_CMD_BUF 512
#define PS_PID_WIDTH 6
#define PS_ABS_WIDTH 10
#define PS_PCT_WIDTH 8

#define PAGE_KIB 4UL
#define KIB_PER_MIB 1024UL
#define KIB_PER_GIB (1024UL * 1024UL)

// Header: black foreground on a light grey background (VGA attribute 0x70).
// ESC 'A' <attribute-byte> sets the console attribute; ESC 'R' resets it.
#define PS_HEADER_COLOR "\x1b" "A" "\x70"
#define PS_COLOR_RESET "\x1b" "R"

typedef struct {
	unsigned long pid;
	unsigned long rss_pages;
	char cmd[PS_CMD_WIDTH + 1];
} ps_task_t;

static const char HELP[] =
	"ps - a pcrutils utility\n\n"
	"ps reports the running tasks, their resident memory and the share of physical memory that memory represents\n\n"
	"args:\n"
	"  -h - prints this help message\n";

static int read_field(const char* path, char* buf, size_t cap) {
	if (cap == 0) {
		return -1;
	}

	int fd = handle_open(path);
	if (fd < 0) {
		return -1;
	}

	int n = handle_read(fd, buf, cap - 1);
	handle_close(fd);

	if (n <= 0) {
		return -1;
	}

	buf[n] = '\0';

	while (n > 0) {
		char c = buf[n - 1];
		if (c != '\n' && c != '\r' && c != ' ' && c != '\t') {
			break;
		}
		
		buf[--n] = '\0';
	}

	return 0;
}

static int read_pages(const char* path, unsigned long* out) {
	char buf[32];

	if (read_field(path, buf, sizeof(buf)) != 0) {
		return -1;
	}

	bool ok;
	long v = pcr_parse_long(buf, &ok);
	if (!ok || v < 0) {
		return -1;
	}

	*out = (unsigned long)v;
	return 0;
}

static bool parse_pid(const char* s, unsigned long* out) {
	bool ok;
	long v = pcr_parse_long(s, &ok);

	if (!ok || v <= 0) {
		return false;
	}

	*out = (unsigned long)v;
	return true;
}

static void pid_path(char* out, size_t cap, unsigned long pid, const char* leaf) {
	const char* base = PS_PID_DIR "/";
	size_t n = 0;

	while (*base && n + 1 < cap) {
		out[n++] = *base++;
	}

	char num[16];
	int k = pcr_uformat(pid, 10, num);
	for (int i = 0; i < k && n + 1 < cap; i++) {
		out[n++] = num[i];
	}

	if (n + 1 < cap) {
		out[n++] = '/';
	}

	while (*leaf && n + 1 < cap) {
		out[n++] = *leaf++;
	}

	out[n] = '\0';
}

static void set_cmd(char* dst, const char* src) {
	size_t n = strlen(src);

	if (n > PS_CMD_WIDTH) {
		n = PS_CMD_WIDTH;
	}

	memcpy(dst, src, n);
	dst[n] = '\0';
}

static int collect_tasks(ps_task_t* tasks, size_t cap, size_t* count) {
	int fd = handle_open(PS_PID_DIR);
	if (fd < 0) {
		printf("pcrutils: ps: %s: cannot open directory\n", PS_PID_DIR);
		return -1;
	}

	dirent_entry_t entry;
	int rc;

	while ((rc = readdir(fd, &entry)) == 0) {
		unsigned long pid;

		if (!parse_pid(entry.name, &pid)) {
			continue;
		}

		if (*count >= cap) {
			break;
		}

		char path[64];
		unsigned long rss_pages;

		pid_path(path, sizeof(path), pid, "rss");
		if (read_pages(path, &rss_pages) != 0) {
			continue;
		}

		ps_task_t* t = &tasks[*count];
		char raw[PS_CMD_BUF];

		t->pid = pid;
		t->rss_pages = rss_pages;

		pid_path(path, sizeof(path), pid, "cmdline");
		if (read_field(path, raw, sizeof(raw)) != 0 || raw[0] == '\0') {
			pid_path(path, sizeof(path), pid, "name");
			if (read_field(path, raw, sizeof(raw)) != 0) {
				set_cmd(t->cmd, "?");
			} else {
				set_cmd(t->cmd, raw);
			}
		} else {
			set_cmd(t->cmd, raw);
		}

		(*count)++;
	}

	handle_close(fd);

	return rc < 0 ? -1 : 0;
}

static void sort_tasks(ps_task_t* tasks, size_t count) {
	for (size_t i = 1; i < count; i++) {
		ps_task_t key = tasks[i];
		size_t j = i;

		while (j > 0 && tasks[j - 1].pid > key.pid) {
			tasks[j] = tasks[j - 1];
			j--;
		}

		tasks[j] = key;
	}
}

static void format_abs(unsigned long kib, char* out) {
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

	int n = pcr_uformat(whole, 10, out);
	char* p = out + n;

	if (tenths != 0) {
		*p++ = '.';
		p += pcr_uformat(tenths, 10, p);
	}

	strcpy(p, unit);
}

static void format_pct(unsigned long rss_pages, unsigned long total_pages, char* out) {
	if (total_pages == 0) {
		strcpy(out, "0.0%");
		return;
	}

	unsigned long tenths = rss_pages * 1000UL / total_pages;

	int n = pcr_uformat(tenths / 10, 10, out);
	char* p = out + n;

	*p++ = '.';
	p += pcr_uformat(tenths % 10, 10, p);
	*p++ = '%';
	*p = '\0';
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	unsigned long total_pages;
	if (read_pages("/sys/meminfo/total", &total_pages) != 0) {
		printf("pcrutils: ps: could not read %s\n", "/sys/meminfo/total");
		return -1;
	}

	ps_task_t tasks[PS_MAX_TASKS];
	size_t count = 0;

	if (collect_tasks(tasks, PS_MAX_TASKS, &count) != 0) {
		return -1;
	}

	sort_tasks(tasks, count);

	size_t cmd_width = strlen("Command");
	for (size_t i = 0; i < count; i++) {
		size_t n = strlen(tasks[i].cmd);
		if (n > cmd_width) {
			cmd_width = n;
		}
	}

	printf(PS_HEADER_COLOR "%-*s %-*s %*s %*s" PS_COLOR_RESET "\n",
		PS_PID_WIDTH, "PID",
		(int)cmd_width, "Command",
		PS_ABS_WIDTH, "Mem(ABS)",
		PS_PCT_WIDTH, "Mem(%)"
	);

	for (size_t i = 0; i < count; i++) {
		char abs[32];
		char pct[16];

		format_abs(tasks[i].rss_pages * PAGE_KIB, abs);
		format_pct(tasks[i].rss_pages, total_pages, pct);

		printf("%-*lu %-*s %*s %*s\n",
			PS_PID_WIDTH, tasks[i].pid,
			(int)cmd_width, tasks[i].cmd,
			PS_ABS_WIDTH, abs,
			PS_PCT_WIDTH, pct
		);
	}

	return 0;
}
