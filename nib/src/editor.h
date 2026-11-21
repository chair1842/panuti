/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef NIB_EDITOR_H
#define NIB_EDITOR_H

#include <stddef.h>
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
#define LINE_LEN_MAX (INT_MAX - 8)

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

typedef struct normalmode_state {
	char pending_op;
	bool pending_replace;
	bool pending_g;
} normalmode_state_t;

typedef struct motion {
	int y;
	int x;
	bool linewise;
	bool inclusive;
} motion_t;

typedef struct range {
	int y1;
	int y2;
	int x2;
	int x1;
	
	bool linewise;
} range_t;

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
	normalmode_state_t nmstate;
	
	bool dirty;

	bool trailing_nl;
} editor_global_t;

typedef enum load_status {
	LOAD_OK,
	LOAD_NEW,
	LOAD_EOOM,
	LOAD_EIO,
	LOAD_ENOTDIR
} load_status_t;

extern editor_global_t EGG;

bool line_reserve(line_t* l, int cap);
bool line_set(line_t* l, const char* str, int len);
bool line_append(line_t* l, char c);
bool line_insert(line_t* l, int pos, char c);
bool line_backspace_at(line_t* l, int pos);
void line_backspace(line_t* l);
void line_free(line_t* l);
void editor_free(void);
line_t* line_new_at(int at);
bool line_delete_at(int at);
bool add_line(const char* str, int len);
void set_errmsg(const char* msg);
load_status_t load(const char* path);
bool save(const char* path);
void goto_line(int y);
void draw(void);

int handin_normal(keypacket_t kp);
int handin_command(keypacket_t kp);
int handin_insert(keypacket_t kp);

#endif
