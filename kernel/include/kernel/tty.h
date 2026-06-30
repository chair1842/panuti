#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <stddef.h>

enum ansi_color {
	ANSI_COLOR_BLACK = 0,
	ANSI_COLOR_BLUE = 1,
	ANSI_COLOR_GREEN = 2,
	ANSI_COLOR_CYAN = 3,
	ANSI_COLOR_RED = 4,
	ANSI_COLOR_MAGENTA = 5,
	ANSI_COLOR_BROWN = 6,
	ANSI_COLOR_LIGHT_GREY = 7,
	ANSI_COLOR_DARK_GREY = 8,
	ANSI_COLOR_LIGHT_BLUE = 9,
	ANSI_COLOR_LIGHT_GREEN = 10,
	ANSI_COLOR_LIGHT_CYAN = 11,
	ANSI_COLOR_LIGHT_RED = 12,
	ANSI_COLOR_LIGHT_MAGENTA = 13,
	ANSI_COLOR_LIGHT_BROWN = 14,
	ANSI_COLOR_WHITE = 15,
};

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void terminal_scroll(void);
void terminal_fsetcolor(enum ansi_color fg_color, enum ansi_color bg_color);
void terminal_clear(void);

#endif
