/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <panuti/handle.h>
#include <panuti/stat.h>
#include <panuti/kbd.h>
#include <panuti/stream.h>

#define ROWS 24
#define COLS 80

typedef struct line {
	char* string;
	int len;
	int cap;
} line_t;

typedef enum editor_mode {
	EM_NORMAL,
	EM_COMMAND,
	EM_INSERT
} editor_mode_t;

typedef struct commandmode_state {
	line_t command;
} commandmode_state_t;

typedef struct editor_global {
	line_t* lines;
	int nlines;
	int capl;

	int vup_x;
	int vup_y;
	
	int cur_x;
	int cur_y;
	
	editor_mode_t mode;
	
	line_t errormsg;
	bool show_errmsg;
	
	commandmode_state_t cmstate;
} editor_global_t;

// why is this called egg?
// i was gonna call it eg for editor global.
// but then the brilliant joke of EGG arrived.
// the name is very annoying to work with.
// but its for the sake of it
static editor_global_t EGG;

static void line_init(line_t* l) {
	l->string = NULL;
	l->len = 0;
	l->cap = 0;
}

static bool line_reserve(line_t* l, int cap) {
	if (l->cap >= cap) {
		return true;
	}

	char* ns = realloc(l->string, cap);
	if (!ns) {
		return false;
	}

	l->string = ns;
	l->cap = cap;
	return true;
}

static bool line_set(line_t* l, const char* str, int len) {
	if (!line_reserve(l, len + 1)) {
		return false;
	}

	memcpy(l->string, str, len);
	l->string[len] = '\0';
	l->len = len;
	return true;
}

static bool line_append(line_t* l, char c) {
	if (!line_reserve(l, l->len + 2)) {
		return false;
	}

	l->string[l->len++] = c;
	l->string[l->len] = '\0';
	return true;
}

static bool line_insert(line_t* l, int pos, char c) {
	if (pos < 0) {
		pos = 0;
	}
	if (pos > l->len) {
		pos = l->len;
	}

	if (!line_reserve(l, l->len + 2)) {
		return false;
	}

	memmove(l->string + pos + 1, l->string + pos, l->len - pos);
	l->string[pos] = c;
	l->len++;
	l->string[l->len] = '\0';
	return true;
}

static void line_backspace(line_t* l) {
	if (l->len > 0) {
		l->string[--l->len] = '\0';
	}
}

static void line_free(line_t* l) {
	free(l->string);
	line_init(l);
}

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
	line_init(l);

	if (!line_set(l, str, len)) {
		return false;
	}

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

int handin_normal(keypacket_t kp) {
	if (kp.keycode == KEYCODE_SEMICOLON && kp.shift == true) {
		EGG.mode = EM_COMMAND;
		return 0;
	} else if (kp.keycode == KEYCODE_ESCAPE) {
		EGG.show_errmsg = false;
		return 0;
	}

	switch (kp.keycode) {
		case KEYCODE_H:
			if (EGG.cur_x > 0) {
				EGG.cur_x--;
			}
			
			break;
		case KEYCODE_L:
			if (EGG.cur_x < EGG.lines[EGG.cur_y].len) {
				EGG.cur_x++;
			}
			
			break;
		case KEYCODE_J:
			if (EGG.cur_y + 1 < EGG.nlines) {
				EGG.cur_y++;
			}
			
			break;
		case KEYCODE_K:
			if (EGG.cur_y > 0) {
				EGG.cur_y--;
			}
			
			break;
		default:
			break;
	}

	return 0;
}

static inline void hicommand_swnorm() {
	line_free(&EGG.cmstate.command);
	EGG.mode = EM_NORMAL;
}

static int hicommand_prcc(void) {
	line_t* command = &EGG.cmstate.command;

	if (command->len == 0) {
		return 0;
	}

	if (strcmp(command->string, "q") == 0) {
		return 1;
	}

	line_free(&EGG.errormsg);
	line_set(&EGG.errormsg, "command \"", 9);

	for (int i = 0; i < command->len; i++) {
		line_append(&EGG.errormsg, command->string[i]);
	}

	const char* tail = "\" is not recognized";
	for (const char* p = tail; *p != '\0'; p++) {
		line_append(&EGG.errormsg, *p);
	}

	return 2;
}

int handin_command(keypacket_t kp) {
	EGG.show_errmsg = false;
	
	if (kp.keycode == KEYCODE_ESCAPE) {
		hicommand_swnorm();
		
		return 0;
	} else if (kp.keycode == KEYCODE_ENTER) {
		int prc = hicommand_prcc();
		hicommand_swnorm();
		
		return prc;
	} else if (kp.keycode == KEYCODE_BACKSPACE) {
		if (EGG.cmstate.command.len == 0) {
			hicommand_swnorm();
			
			return 0;
		}

		line_backspace(&EGG.cmstate.command);
		
		return 0;
	}

	char c = keypacket_to_ascii(kp);
	if (c != 0) {
		line_append(&EGG.cmstate.command, c);
	}
	
	return 0;
}

// draw's job is to draw to the screen
// all it must do is take in the current ui state
// it shouldnt write anything, just display
static void draw(void) {
	char buf[(ROWS + 1) * (4 + COLS) + 64];
	size_t n = 0;

	buf[n++] = 0x1B;
	buf[n++] = 'V';
	buf[n++] = 0;

	for (int r = 0; r < ROWS; r++) {
		buf[n++] = 0x1B;
		buf[n++] = 'M'; 
		buf[n++] = 0;
		buf[n++] = (char)r;

		int row = EGG.vup_y + r;
		int len = 0;
		
		if (row < EGG.nlines) {
			int start = EGG.vup_x;
			int avail = EGG.lines[row].len - start;
			if (avail < 0) {
				avail = 0;
			}

			len = avail < COLS ? avail : COLS;
			for (int i = 0; i < len; i++) {
				unsigned char c = (unsigned char)EGG.lines[row].string[start + i];
				buf[n + i] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
			}
		}
		
		memset(buf + n + len, ' ', COLS - len);
		n += COLS;
	}

	buf[n++] = 0x1B;
	buf[n++] = 'M';
	buf[n++] = 0;
	buf[n++] = (char)ROWS;

	bool err = EGG.mode != EM_COMMAND && EGG.show_errmsg;

	if (err) {
		buf[n++] = 0x1B;
		buf[n++] = 'A';
		buf[n++] = 0x47;
	}

	int slen = 0;
	if (EGG.mode == EM_COMMAND) {
		buf[n++] = ':';
		slen++;

		for (int i = 0; i < EGG.cmstate.command.len && slen < COLS; i++) {
			unsigned char c = (unsigned char)EGG.cmstate.command.string[i];
			buf[n++] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
			slen++;
		}
	} else if (EGG.show_errmsg) {
		for (int i = 0; i < EGG.errormsg.len && slen < COLS; i++) {
			unsigned char c = (unsigned char)EGG.errormsg.string[i];
			buf[n++] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
			slen++;
		}
	}

	if (err) {
		buf[n++] = 0x1B;
		buf[n++] = 'A';
		buf[n++] = 0x07;
	}

	memset(buf + n, ' ', COLS - slen);
	n += COLS - slen;

	const char* mode_str;
	switch (EGG.mode) {
		case EM_COMMAND:
			mode_str = "COMMAND";
			break;
		case EM_INSERT:
			mode_str = "INSERT";
			break;
		default:
			mode_str = "NORMAL";
			break;
	}

	int mode_len = (int)strlen(mode_str);
	int mode_col = COLS - mode_len;

	if (mode_col < 0) {
		mode_col = 0;
	}

	buf[n++] = 0x1B;
	buf[n++] = 'M';
	buf[n++] = (char)mode_col;
	buf[n++] = (char)ROWS;

	for (const char* p = mode_str; *p != '\0'; p++) {
		buf[n++] = *p;
	}

	int ccx = EGG.cur_x - EGG.vup_x;
	int ccy = EGG.cur_y - EGG.vup_y;

	if (EGG.mode == EM_COMMAND) {
		ccx = 1 + EGG.cmstate.command.len;
		ccy = ROWS;

		if (ccx >= COLS) {
			ccx = COLS - 1;
		}
	}

	buf[n++] = 0x1B;
	buf[n++] = 'M';
	buf[n++] = (char)ccx;
	buf[n++] = (char)ccy;

	buf[n++] = 0x1B;
	buf[n++] = 'V';
	buf[n++] = 1;

	stream_write(0, buf, n);
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("nib: usage: nib <file>\n");
		return -1;
	}
	
	if (strcmp(argv[1], "-h") == 0) {
		printf("nib 0.1\n");
		printf("usage: nib <file>\n");

		return 0;
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
	stream_write(0, (char[]){ 0x1B, 'W', 0 }, 3);

	// main loop
	while (1) {
		draw();
		
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

		// mode handling values
		// 0 = continue
		// 1 = quit
		// 2 = error
		int hc = 0;
		
		switch (EGG.mode) {
			case EM_NORMAL:
				hc = handin_normal(kp);
				break;
			case EM_COMMAND:
				hc = handin_command(kp);
				break;
			default:
				hc = 1;
				break;
		}

		if (hc == 1) {
			break;
		} else if (hc == 2) {
			EGG.show_errmsg = true;
		}
		
		if (EGG.cur_x > EGG.lines[EGG.cur_y].len) {
			EGG.cur_x = EGG.lines[EGG.cur_y].len;
		}

		if (EGG.cur_y < EGG.vup_y) {
			EGG.vup_y = EGG.cur_y;
		}

		if (EGG.cur_y >= EGG.vup_y + ROWS) {
			EGG.vup_y = EGG.cur_y - ROWS + 1;
		}

		if (EGG.cur_x < EGG.vup_x) {
			EGG.vup_x = EGG.cur_x;
		}

		if (EGG.cur_x >= EGG.vup_x + COLS) {
			EGG.vup_x = EGG.cur_x - COLS + 1;
		}
	}

	stream_write(0, (char[]){ 0x1B, 'R' }, 2);
	printf("\f");
	handle_close(kbd);
	
	return 0;
}