/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <panuti/handle.h>
#include <panuti/stat.h>
#include <panuti/kbd.h>

#define ROWS 24
#define COLS 80

typedef struct line {
	char* string;
	int len;
	int cap;
} line_t;

typedef struct editor_global {
	line_t* lines;
	int nlines;
	int capl;

	int vup_y;
	int vup_x;
} editor_global_t;

// why is this called egg?
// i was gonna call it eg for editor global.
// but then the brilliant joke of EGG arrived.
// the name is very annoying to work with.
// but its for the sake of it
static editor_global_t EGG;

static bool add_line(const char* str, int len) {
	if (EGG.nlines == EGG.capl) {
		int ncap = EGG.capl ? EGG.capl * 2 : 64;
		line_t* nl = realloc(EGG.lines, ncap * sizeof(line_t));
		if (!nl) {
			return false;
		}

		EGG.lines = nl;
		EGG.capl = ncap;
	}

	line_t* l = &EGG.lines[EGG.nlines];
	
	l->cap = len + 1;
	l->string = malloc(l->cap);
	if (!l->string) {
		return false;
	}

	memcpy(l->string, str, len);
	l->string[len] = '\0';
	l->len = len;
	EGG.nlines++;

	return true;
}

bool load(const char* path) {
	dirent_entry_t dr;
	if (!nexist(path)) {
		return add_line("", 0);
	}

	if (stat(path, &dr) != 0) {
		return add_line("", 0);
	}

	size_t size = dr.size;
	char* data = malloc(size + 1);
	if (!data) {
		return false;
	}

	int fd = handle_open(path);
	if (fd < 0) {
		free(data);
		return false;
	}

	size_t b = 0;
	while (b < size) {
		int n = handle_read(fd, data + b, size - b);
		if (n <= 0) {
			break;
		}
	
		b += n;
	}
	
	handle_close(fd);

	size_t start = 0;
	for (size_t i = 0; i < b; i++) {
		if (data[i] == '\n') {
			if (!add_line(data + start, (int)(i - start))) {
				free(data);
				return false;
			}
			
			start = i + 1;
		}
	}
	
	if (start < b) {
		if (!add_line(data + start, (int)(b - start))) {
			free(data);
			return false;
		}
	}
	
	if (EGG.nlines == 0) {
		if (!add_line("", 0)) {
			free(data);
			return false;
		}
	}

	free(data);
	return true;
}

static void draw(void) {
	char buf[ROWS * (4 + COLS) + 1];
	size_t n = 0;

	for (int r = 0; r < ROWS; r++) {
		buf[n++] = 0x1B;
		buf[n++] = 'M'; 
		buf[n++] = 0;
		buf[n++] = (char)r;

		int row = EGG.vup_y + r;
		int len = 0;
		
		if (row < EGG.nlines) {
			len = EGG.lines[row].len < COLS ? EGG.lines[row].len : COLS;
			for (int i = 0; i < len; i++) {
				unsigned char c = (unsigned char)EGG.lines[row].string[i];
				buf[n + i] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
			}
		}
		
		memset(buf + n + len, ' ', COLS - len);
		n += COLS;
	}
	
	buf[n] = '\0';
	printf("%s", buf);
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("nib: usage: nib <file>\n");
		return -1;
	}

	if (!load(argv[1])) {
		printf("nib: could not open \"%s\"\n", argv[1]);
		return -1;
	}

	int kbd = handle_open("/dvc/kbd/raw");
	if (kbd < 0) {
		printf("nib: keyboard cannot be opened\n");
		return -1;
	}

	printf("\f");
	printf("%s", (char[]){ 0x1B, 'W', 0 });
	draw();

	while (1) {
		keypacket_t kp;
		int n = handle_read(kbd, &kp, sizeof(kp));

		if (n < 0) {
			break;
		}

		if (n != (int)(sizeof(kp))) {
			continue;
		}

		if (!kp.pressed) {
			continue;
		}

		if (kp.keycode == KEYCODE_Q) {
			break;
		}
	}

	printf("%s", (char[]){ 0x1B, 'R', 0 });
	printf("\f");
	handle_close(kbd);
	
	return 0;
}