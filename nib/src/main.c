/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <panuti/handle.h>
#include <panuti/stat.h>
#include <panuti/kbd.h>
#include <panuti/stream.h>
#include <panuti/errno.h>
#include <panuti/syscall/syscallsf.h>

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

	const char* path;

	int vup_x;
	int vup_y;
	
	int cur_x;
	int cur_y;
	int insert_x;
	
	editor_mode_t mode;
	
	line_t errormsg;
	bool show_errmsg;
	
	commandmode_state_t cmstate;
	
	bool dirty;

	bool trailing_nl;
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

#define LINE_LEN_MAX (INT_MAX - 8)

static bool line_reserve(line_t* l, int cap) {
	if (cap < 0) {
		return false;
	}

	if (l->cap >= cap) {
		return true;
	}

	int ncap = l->cap ? l->cap : 16;
	while (ncap < cap) {
		if (ncap > INT_MAX / 2) {
			ncap = cap;
			break;
		}

		ncap *= 2;
	}

	char* ns = realloc(l->string, (size_t)ncap);
	if (!ns) {
		return false;
	}

	l->string = ns;
	l->cap = ncap;
	return true;
}

static bool line_set(line_t* l, const char* str, int len) {
	if (len < 0 || len > LINE_LEN_MAX) {
		return false;
	}

	if (!line_reserve(l, len + 1)) {
		return false;
	}

	memcpy(l->string, str, len);
	l->string[len] = '\0';
	l->len = len;
	return true;
}

static bool line_append(line_t* l, char c) {
	if (l->len >= LINE_LEN_MAX) {
		return false;
	}

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

	if (l->len >= LINE_LEN_MAX) {
		return false;
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

static bool line_backspace_at(line_t* l, int pos) {
	if (pos < 0 || pos >= l->len) {
		return false;
	}

	memmove(l->string + pos, l->string + pos + 1, (size_t)(l->len - pos - 1));
	l->len--;
	l->string[l->len] = '\0';
	return true;
}

static void line_backspace(line_t* l) {
	line_backspace_at(l, l->len - 1);
}

static void line_free(line_t* l) {
	free(l->string);
	line_init(l);
}

static line_t* line_new_at(int at) {
	if (EGG.nlines == EGG.capl) {
		int ncap = EGG.capl ? EGG.capl * 2 : 64;
		line_t* nl = realloc(EGG.lines, ncap * sizeof(line_t));
		if (!nl) {
			return NULL;
		}

		EGG.lines = nl;
		EGG.capl = ncap;
	}

	if (at < 0) {
		at = 0;
	}
	if (at > EGG.nlines) {
		at = EGG.nlines;
	}

	memmove(&EGG.lines[at + 1], &EGG.lines[at], (size_t)(EGG.nlines - at) * sizeof(line_t));

	line_init(&EGG.lines[at]);
	EGG.nlines++;

	return &EGG.lines[at];
}

static bool line_delete_at(int at) {
	if (at < 0 || at >= EGG.nlines) {
		return false;
	}

	line_free(&EGG.lines[at]);

	memmove(&EGG.lines[at], &EGG.lines[at + 1], (size_t)(EGG.nlines - at - 1) * sizeof(line_t));

	EGG.nlines--;
	return true;
}

static bool add_line(const char* str, int len) {
	line_t* l = line_new_at(EGG.nlines);
	if (!l) {
		return false;
	}

	if (!line_set(l, str, len)) {
		line_delete_at(EGG.nlines - 1);
		return false;
	}

	return true;
}

static void set_errmsg(const char* msg) {
	line_set(&EGG.errormsg, msg, (int)strlen(msg));
	EGG.show_errmsg = true;
}

static char* tmp_path(const char* path) {
	size_t len = strlen(path);
	char* tmp = malloc(len + 6);
	if (!tmp) {
		return NULL;
	}

	memcpy(tmp, path, len);
	memcpy(tmp + len, ".ntmp", 6);
	return tmp;
}

typedef enum load_status {
	LOAD_OK,
	LOAD_NEW,
	LOAD_EOOM,
	LOAD_EIO,
	LOAD_ENOTDIR
} load_status_t;

static load_status_t load(const char* path) {
	char* stale = tmp_path(path);
	if (stale) {
		if (nexist(stale)) {
			panutisysf_unlink(stale);
		}

		free(stale);
	}

	dirent_entry_t dr;
	int32_t sr = stat(path, &dr);

	if (sr != 0) {
		if (sr != (int32_t)PANUTIERRNO_NOTFOUND) {
			return LOAD_EIO;
		}

		EGG.trailing_nl = true;

		if (!add_line("", 0)) {
			return LOAD_EOOM;
		}

		return LOAD_NEW;
	}

	if (dr.type == INODE_DIR) {
		return LOAD_ENOTDIR;
	}

	size_t size = dr.size;
	char* data = malloc(size + 1);
	if (!data) {
		return LOAD_EOOM;
	}

	int fd = handle_open(path);
	if (fd < 0) {
		free(data);
		return LOAD_EIO;
	}

	size_t b = 0;
	while (b < size) {
		int n = handle_read(fd, data + b, size - b);
		if (n < 0) {
			handle_close(fd);
			free(data);
			return LOAD_EIO;
		}

		if (n == 0) {
			break;
		}

		b += (size_t)n;
	}

	handle_close(fd);

	if (b < size) {
		free(data);
		return LOAD_EIO;
	}

	size_t start = 0;
	for (size_t i = 0; i < b; i++) {
		if (data[i] == '\n') {
			if (!add_line(data + start, (int)(i - start))) {
				free(data);
				return LOAD_EOOM;
			}

			start = i + 1;
		}
	}

	if (start < b) {
		if (!add_line(data + start, (int)(b - start))) {
			free(data);
			return LOAD_EOOM;
		}
	}

	if (EGG.nlines == 0) {
		if (!add_line("", 0)) {
			free(data);
			return LOAD_EOOM;
		}
	}

	EGG.trailing_nl = (b > 0 && data[b - 1] == '\n');

	free(data);
	return LOAD_OK;
}

static bool write_all(int fd, const void* data, size_t len) {
	const char* p = data;
	size_t off = 0;

	while (off < len) {
		int w = handle_write(fd, p + off, len - off);
		if (w <= 0) {
			return false;
		}

		off += (size_t)w;
	}

	return true;
}

bool save(const char* path) {
	if (!EGG.dirty && nexist(path)) {
		return true;
	}

	char* tmp = tmp_path(path);
	if (!tmp) {
		return false;
	}

	if (!nexist(tmp)) {
		if (panutisysf_mkfile(tmp) < 0) {
			free(tmp);
			return false;
		}
	}

	int fd = handle_open(tmp);
	if (fd < 0) {
		free(tmp);
		return false;
	}

	uint64_t zero = 0;

	if (panutisysf_resize(fd, &zero) < 0) {
		handle_close(fd);
		panutisysf_unlink(tmp);
		free(tmp);
		return false;
	}

	bool ok = true;

	for (int i = 0; i < EGG.nlines && ok; i++) {
		if (EGG.lines[i].len > 0) {
			ok = write_all(fd, EGG.lines[i].string, (size_t)EGG.lines[i].len);
		}

		if (ok && (i + 1 < EGG.nlines || EGG.trailing_nl)) {
			char nl = '\n';
			ok = write_all(fd, &nl, 1);
		}
	}

	handle_close(fd);

	if (ok) {
		ok = panutisysf_rename(tmp, path) == 0;
	}

	if (!ok) {
		panutisysf_unlink(tmp);
	}

	free(tmp);

	if (ok) {
		EGG.dirty = false;
	}

	return ok;
}

int handin_normal(keypacket_t kp) {
	if (kp.keycode == KEYCODE_SEMICOLON && kp.shift == true) {
		EGG.mode = EM_COMMAND;
		return 0;
	} else if (kp.keycode == KEYCODE_ESCAPE) {
		EGG.show_errmsg = false;
		return 0;
	} else if (kp.keycode == KEYCODE_ENTER) {
		line_t* l = line_new_at(EGG.cur_y + 1);
		if (!l) {
			set_errmsg("out of memory");
			return 2;
		}

		EGG.dirty = true;
		EGG.cur_y++;
		EGG.cur_x = 0;

		return 0;
	} else if (kp.keycode == KEYCODE_O && !kp.shift) {
		line_t* l = line_new_at(EGG.cur_y + 1);
		if (!l) {
			set_errmsg("out of memory");
			return 2;
		}

		EGG.dirty = true;
		EGG.cur_y++;
		EGG.cur_x = 0;
		EGG.insert_x = 0;
		EGG.mode = EM_INSERT;

		return 0;
	} else if (kp.keycode == KEYCODE_O) {
		line_t* l = line_new_at(EGG.cur_y);
		if (!l) {
			set_errmsg("out of memory");
			return 2;
		}

		EGG.dirty = true;
		EGG.cur_x = 0;
		EGG.insert_x = 0;
		EGG.mode = EM_INSERT;

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
		case KEYCODE_I:
			if (!kp.shift) {
				EGG.insert_x = EGG.cur_x;
				EGG.mode = EM_INSERT;
			}
			
			break;
		case KEYCODE_A:
			if (kp.shift) {
				EGG.cur_x = EGG.lines[EGG.cur_y].len;
			} else if (EGG.cur_x < EGG.lines[EGG.cur_y].len) {
				EGG.cur_x++;
			}
			
			EGG.insert_x = EGG.cur_x;
			EGG.mode = EM_INSERT;
			
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
		if (EGG.dirty) {
			set_errmsg("unsaved changes, use \":q!\" to override");
			return 2;
		}

		return 1;
	} else if (strcmp(command->string, "w") == 0) {
		if (!save(EGG.path)) {
			set_errmsg("could not save the file");
			return 2;
		}

		return 0;
	} else if (strcmp(command->string, "wq") == 0) {
		if (!save(EGG.path)) {
			set_errmsg("could not save the file");
			return 2;
		}

		return 1;
	} else if (strcmp(command->string, "q!") == 0) {
		return 1;
	}

	line_set(&EGG.errormsg, "command \"", 9);

	for (int i = 0; i < command->len; i++) {
		if (!line_append(&EGG.errormsg, command->string[i])) {
			break;
		}
	}

	const char* tail = "\" is not recognized";
	for (const char* p = tail; *p != '\0'; p++) {
		if (!line_append(&EGG.errormsg, *p)) {
			break;
		}
	}

	EGG.show_errmsg = true;
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
		if (!line_append(&EGG.cmstate.command, c)) {
			set_errmsg("out of memory");
			return 2;
		}
	}
	
	return 0;
}

int handin_insert(keypacket_t kp) {
	switch (kp.keycode) {
		case KEYCODE_ESCAPE:
			EGG.mode = EM_NORMAL;
			if (EGG.cur_x > EGG.insert_x) {
				EGG.cur_x--;
			}

			return 0;
		case KEYCODE_ENTER:
			int at = EGG.cur_x;
			char* str = EGG.lines[EGG.cur_y].string;
			int len = EGG.lines[EGG.cur_y].len;
		
			line_t* nl = line_new_at(EGG.cur_y + 1);
			if (!nl) {
				set_errmsg("out of memory");
				return 0;
			}
		
			if (!line_set(nl, str + at, len - at)) {
				line_delete_at(EGG.cur_y + 1);
				set_errmsg("out of memory");
				return 0;
			}
		
			EGG.lines[EGG.cur_y].len = at;
			str[at] = '\0';
		
			EGG.cur_y++;
			EGG.cur_x = 0;
			EGG.insert_x = 0;
			EGG.dirty = true;
		
			return 0;
		case KEYCODE_BACKSPACE:
			if (EGG.cur_x > 0) {
				line_backspace_at(&EGG.lines[EGG.cur_y], EGG.cur_x - 1);
				EGG.cur_x--;
				EGG.dirty = true;
			} else if (EGG.cur_y > 0) {
				line_t* prev = &EGG.lines[EGG.cur_y - 1];
				line_t* cur = &EGG.lines[EGG.cur_y];
				int at = prev->len;

				if (!line_reserve(prev, prev->len + cur->len + 1)) {
					set_errmsg("out of memory");
					return 0;
				}

				for (int i = 0; i < cur->len; i++) {
					line_append(prev, cur->string[i]);
				}

				line_delete_at(EGG.cur_y);
				EGG.cur_y--;
				EGG.cur_x = at;
				EGG.insert_x = at;
				EGG.dirty = true;
			}

			return 0;
		case KEYCODE_TAB:
			if (line_insert(&EGG.lines[EGG.cur_y], EGG.cur_x, '\t')) {
				EGG.cur_x++;
				EGG.dirty = true;
			} else {
				set_errmsg("out of memory");
			}

			return 0;
		default:
			break;
	}

	char c = keypacket_to_ascii(kp);
	if (c != 0) {
		if (line_insert(&EGG.lines[EGG.cur_y], EGG.cur_x, c)) {
			EGG.cur_x++;
			EGG.dirty = true;
		} else {
			set_errmsg("out of memory");
		}
	}
	
	return 0;
}

static int line_width(const line_t* l, int start, int x) {
	int col = 0;

	for (int i = start; i < x && i < l->len; i++) {
		if (l->string[i] == '\t') {
			col = (col / 8 + 1) * 8;
		} else {
			col++;
		}
	}

	return col;
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
		int col = 0;

		if (row < EGG.nlines) {
			line_t* l = &EGG.lines[row];

			for (int i = EGG.vup_x; i < l->len && col < COLS; i++) {
				unsigned char c = (unsigned char)l->string[i];

				if (c == '\t') {
					int next = (col / 8 + 1) * 8;
					while (col < next && col < COLS) {
						buf[n + col++] = ' ';
					}
				} else {
					buf[n + col++] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
				}
			}
		}

		memset(buf + n + col, ' ', COLS - col);
		n += COLS;
	}

	buf[n++] = 0x1B;
	buf[n++] = 'M';
	buf[n++] = 0;
	buf[n++] = (char)ROWS;

	buf[n++] = 0x1B;
	buf[n++] = 'A';
	buf[n++] = 0x70;

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

	int lim = mode_col - 1;
	if (lim < 0) {
		lim = 0;
	}

	bool err = EGG.mode != EM_COMMAND && EGG.show_errmsg;

	if (err) {
		buf[n++] = 0x1B;
		buf[n++] = 'A';
		buf[n++] = 0x4F;
	}

	int slen = 0;
	if (EGG.mode == EM_COMMAND) {
		buf[n++] = ':';
		slen++;

		for (int i = 0; i < EGG.cmstate.command.len && slen < lim; i++) {
			unsigned char c = (unsigned char)EGG.cmstate.command.string[i];
			buf[n++] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
			slen++;
		}
	} else if (EGG.show_errmsg) {
		for (int i = 0; i < EGG.errormsg.len && slen < lim; i++) {
			unsigned char c = (unsigned char)EGG.errormsg.string[i];
			buf[n++] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
			slen++;
		}
	} else {
		const char* name = EGG.path;

		for (const char* p = EGG.path; *p != '\0'; p++) {
			if (*p == '/') {
				name = p + 1;
			}
		}

		for (const char* p = name; *p != '\0' && slen < lim; p++) {
			unsigned char c = (unsigned char)*p;
			buf[n++] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
			slen++;
		}

		if (EGG.dirty && slen < lim) {
			buf[n++] = '*';
			slen++;
		}
	}

	if (err) {
		buf[n++] = 0x1B;
		buf[n++] = 'A';
		buf[n++] = 0x70;
	}

	memset(buf + n, ' ', COLS - slen);
	n += COLS - slen;

	buf[n++] = 0x1B;
	buf[n++] = 'M';
	buf[n++] = (char)mode_col;
	buf[n++] = (char)ROWS;

	for (const char* p = mode_str; *p != '\0'; p++) {
		buf[n++] = *p;
	}

	buf[n++] = 0x1B;
	buf[n++] = 'A';
	buf[n++] = 0x07;

	int ccx;
	int ccy = EGG.cur_y - EGG.vup_y;

	if (EGG.mode == EM_COMMAND) {
		ccx = 1 + EGG.cmstate.command.len;
		ccy = ROWS;
	} else {
		ccx = line_width(&EGG.lines[EGG.cur_y], EGG.vup_x, EGG.cur_x);
	}

	if (ccx >= COLS) {
		ccx = COLS - 1;
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

	EGG.path = argv[1];

	switch (load(EGG.path)) {
		case LOAD_OK:
		case LOAD_NEW:
			break;
		case LOAD_EOOM:
			printf("nib: out of memory loading \"%s\"\n", argv[1]);
			return -1;
		case LOAD_ENOTDIR:
			printf("nib: \"%s\" is a directory\n", argv[1]);
			return -1;
		case LOAD_EIO:
		default:
			printf("nib: could not read \"%s\"\n", argv[1]);
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
			// a read error is no reason to throw away unsaved edits, so report
			// it and keep the editor running
			set_errmsg("keyboard read error");
			panutisysf_yield();
			continue;
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
			case EM_INSERT:
				hc = handin_insert(kp);
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