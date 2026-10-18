/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <panuti/handle.h>
#include <panuti/stat.h>
#include <panuti/stream.h>
#include <panuti/syscall/syscallsf.h>

#include "life.h"

#define STATUS_ROW LIFE_H
#define SCREEN_H (LIFE_H + 1)

#define DEFAULT_STEPS 300
#define DEFAULT_FPS 60
#define MIN_FPS 1
#define MAX_FPS 60

#define ESC 0x1B

#define ATTR_ALIVE 0x0A // light green on black
#define ATTR_STATUS 0x07 // light grey on black

static const char ESC_HIDE_CURSOR[] = { ESC, 'V', 0 };
static const char ESC_NO_WRAP[] = { ESC, 'W', 0 };
static const char ESC_RESET[] = { ESC, 'R' };

static unsigned char cur[LIFE_CELLS];
static unsigned char next[LIFE_CELLS];

static const char HELP[] =
	"conway - Conway's Game of Life\n\n"
	"usage:\n"
	"  conway [options]\n\n"
	"options:\n"
	"  -n <steps> - how many steps the simulation runs for (default 300)\n"
	"  -f <fps> - simulation speed, 1-60 (default 60)\n"
	"  -s <path> - save the final board state to a file\n"
	"  -p - read the starting board from the in stream\n"
	"  -h - print this help message\n\n"
	"the board is 80x24 and wraps around at the edges.\n"
	"without -p it starts from a random state.\n";

// a sink so fctprintf can append straight into the render buffer
typedef struct sink {
	char* p;
	size_t n;
} sink_t;

static void sink_put(char c, void* extra) {
	sink_t* s = (sink_t*)extra;
	s->p[s->n++] = c;
}

static int uptime_read(int fd, long* out) {
	char buf[32];
	int n = handle_read(fd, buf, sizeof(buf) - 1);
	if (n < 0) {
		return -1;
	}

	long v = 0;
	for (int i = 0; i < n && buf[i] >= '0' && buf[i] <= '9'; i++) {
		v = v * 10 + (long)(buf[i] - '0');
	}

	*out = v;
	return 0;
}

static int frame_wait(int uptime_fd, int fps, long* base, long* acc) {
	*acc += (100L << 8) / fps;
	long ticks = *acc >> 8;
	*acc &= 0xFF;

	long target = *base + ticks;

	for (;;) {
		long now;
		if (uptime_read(uptime_fd, &now) != 0) {
			return -1;
		}

		if (now >= target) {
			break;
		}

		panutisysf_yield();
	}

	*base = target;
	return 0;
}

static void render(long gen, long steps, int alive, int fps) {
	static char buf[SCREEN_H * (4 + LIFE_W) + 4 + 3 + LIFE_W + 4];

	size_t n = 0;
	memcpy(buf + n, ESC_HIDE_CURSOR, sizeof(ESC_HIDE_CURSOR));
	n += sizeof(ESC_HIDE_CURSOR);

	buf[n++] = ESC;
	buf[n++] = 'A';
	buf[n++] = ATTR_ALIVE;

	for (int y = 0; y < LIFE_H; y++) {
		buf[n++] = ESC;
		buf[n++] = 'M';
		buf[n++] = 0;
		buf[n++] = (char)y;

		for (int x = 0; x < LIFE_W; x++) {
			buf[n++] = cur[y * LIFE_W + x] ? (char)0xDB : ' ';
		}
	}

	buf[n++] = ESC;
	buf[n++] = 'M';
	buf[n++] = 0;
	buf[n++] = STATUS_ROW;

	buf[n++] = ESC;
	buf[n++] = 'A';
	buf[n++] = ATTR_STATUS;

	size_t status = n;
	sink_t s = { buf, n };
	fctprintf(sink_put, &s, "gen %ld/%ld  alive %d  %d fps", gen, steps, alive, fps);
	n = s.n;

	while (n - status < LIFE_W) {
		buf[n++] = ' ';
	}

	stream_write(0, buf, n);
}

static void load_start(void) {
	static char data[8192];

	size_t len = 0;
	while (len < sizeof(data)) {
		int r = stream_read(0, data + len, sizeof(data) - len);
		if (r <= 0) {
			break;
		}
		
		len += (size_t)r;
	}

	life_parse(data, len, cur);
}

static int init_random(void) {
	int fd = handle_open("/dvc/random");
	if (fd < 0) {
		return -1;
	}

	size_t want = LIFE_CELLS;
	size_t got = 0;

	while (got < want) {
		int r = handle_read(fd, cur + got, want - got);
		if (r <= 0) {
			handle_close(fd);
			return -1;
		}

		got += (size_t)r;
	}

	handle_close(fd);

	for (int i = 0; i < LIFE_CELLS; i++) {
		cur[i] = (cur[i] % 100) < 30;
	}

	return 0;
}

static bool write_all(int fd, const char* data, size_t len) {
	size_t off = 0;

	while (off < len) {
		int w = handle_write(fd, data + off, len - off);
		if (w <= 0) {
			return false;
		}

		off += (size_t)w;
	}

	return true;
}

static int save_state(const char* path) {
	if (!nexist(path)) {
		if (panutisysf_mkfile(path) < 0) {
			return -1;
		}
	}

	int fd = handle_open(path);
	if (fd < 0) {
		return -1;
	}

	uint64_t zero = 0;
	if (panutisysf_resize(fd, &zero) < 0) {
		handle_close(fd);
		return -1;
	}

	char text[LIFE_H * (LIFE_W + 1)];
	life_format(cur, text);

	bool ok = write_all(fd, text, sizeof(text));

	handle_close(fd);
	return ok ? 0 : -1;
}

static bool parse_long(const char* text, long* out) {
	if (*text == '\0') {
		return false;
	}

	long v = 0;

	for (const char* p = text; *p; p++) {
		if (*p < '0' || *p > '9') {
			return false;
		}

		v = v * 10 + (*p - '0');
	}

	*out = v;
	return true;
}

int main(int argc, char** argv) {
	long steps = DEFAULT_STEPS;
	int fps = DEFAULT_FPS;
	const char* save_path = NULL;
	bool use_start = false;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (strcmp(a, "-h") == 0) {
			printf("%s", HELP);
			return 0;
		} else if (strcmp(a, "-p") == 0) {
			use_start = true;
		} else if (strcmp(a, "-n") == 0) {
			if (i + 1 >= argc || !parse_long(argv[++i], &steps)) {
				printf("conway: -n needs a whole number of steps\n");
				return -1;
			}
		} else if (strcmp(a, "-f") == 0) {
			long f;

			if (i + 1 >= argc || !parse_long(argv[++i], &f)) {
				printf("conway: -f needs a whole number of frames\n");
				return -1;
			}

			if (f < MIN_FPS || f > MAX_FPS) {
				printf("conway: fps must be between %d and %d\n", MIN_FPS, MAX_FPS);
				return -1;
			}

			fps = (int)f;
		} else if (strcmp(a, "-s") == 0) {
			if (i + 1 >= argc) {
				printf("conway: -s needs a path\n");
				return -1;
			}

			save_path = argv[++i];
		} else {
			printf("conway: unknown argument '%s'\n", a);
			printf("%s", HELP);
			return -1;
		}
	}

	if (use_start) {
		load_start();
	} else if (init_random() != 0) {
		printf("conway: cannot read /dvc/random\n");
		return -1;
	}

	int uptime_fd = handle_open("/dvc/uptime");
	if (uptime_fd < 0) {
		printf("conway: cannot read /dvc/uptime\n");
		return -1;
	}

	long base;
	if (uptime_read(uptime_fd, &base) != 0) {
		printf("conway: cannot read /dvc/uptime\n");
		handle_close(uptime_fd);
		return -1;
	}

	int alive = life_count(cur);

	printf("\f");
	stream_write(0, ESC_NO_WRAP, sizeof(ESC_NO_WRAP));

	long acc = 0;
	render(0, steps, alive, fps);

	for (long gen = 1; gen <= steps; gen++) {
		if (frame_wait(uptime_fd, fps, &base, &acc) != 0) {
			break;
		}

		alive = life_step(cur, next);
		memcpy(cur, next, sizeof(cur));
		render(gen, steps, alive, fps);
	}

	handle_close(uptime_fd);

	if (save_path && save_state(save_path) != 0) {
		printf("conway: could not save to '%s'\n", save_path);
	}

	stream_write(0, ESC_RESET, sizeof(ESC_RESET));
	printf("\f");
	printf("conway: ran %ld steps, %d cells alive\n", steps, alive);

	return 0;
}
