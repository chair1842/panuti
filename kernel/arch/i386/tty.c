/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/tty.h>

#include "vga.h"
#include "io.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xC00B8000;

#define VGA_CRT_INDEX 0x3D4
#define VGA_CRT_DATA 0x3D5

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

#define ESC_BYTE 0x1B

typedef enum {
	ESC_IDLE = 0,
	ESC_PREFIX,
	ESC_M_COLUMN,
	ESC_M_ROW,
	ESC_A_ATTR,
	ESC_V_ARG,
	ESC_W_ARG, 
} esc_state_t;

static esc_state_t esc_state = ESC_IDLE;
static uint8_t esc_column;
static uint8_t terminal_scroll_wrap = 1;

static void terminal_update_cursor(void) {
	const size_t pos = terminal_row * VGA_WIDTH + terminal_column;

	outb(VGA_CRT_INDEX, 0x0E);
	outb(VGA_CRT_DATA, (uint8_t)((pos >> 8) & 0xFF));
	outb(VGA_CRT_INDEX, 0x0F);
	outb(VGA_CRT_DATA, (uint8_t)(pos & 0xFF));
}

void terminal_initialize(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = VGA_MEMORY;
	terminal_scroll_wrap = 1;
	esc_state = ESC_IDLE;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}

	outb(VGA_CRT_INDEX, 0x0A);
	outb(VGA_CRT_DATA, 0x00);
	outb(VGA_CRT_INDEX, 0x0B);
	outb(VGA_CRT_DATA, 0x0F);
	terminal_update_cursor();
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

static void terminal_advance_row(void) {
	if (++terminal_row == VGA_HEIGHT) {
		if (terminal_scroll_wrap) {
			terminal_scroll();
		}
		terminal_row = VGA_HEIGHT - 1;
	}
}

static void terminal_wrap_or_stick(void) {
	if (terminal_scroll_wrap) {
		terminal_column = 0;
		terminal_advance_row();
	} else {
		terminal_column = VGA_WIDTH - 1;
	}
}

static void terminal_cursor_setvisible(uint8_t visible) {
	outb(VGA_CRT_INDEX, 0x0A);
	outb(VGA_CRT_DATA, visible ? 0x00 : 0x20); /* bit 5 disables the shape */
	if (visible) {
		outb(VGA_CRT_INDEX, 0x0B);
		outb(VGA_CRT_DATA, 0x0F);
	}
	terminal_update_cursor();
}

static void terminal_escape(uint8_t byte) {
	switch (esc_state) {
	case ESC_PREFIX:
		switch (byte) {
		case 'M':
			esc_state = ESC_M_COLUMN;
			break;
		case 'A':
			esc_state = ESC_A_ATTR;
			break;
		case 'V':
			esc_state = ESC_V_ARG;
			break;
		case 'W':
			esc_state = ESC_W_ARG;
			break;
		case 'R':
			terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
			terminal_scroll_wrap = 1;
			terminal_cursor_setvisible(1);
			esc_state = ESC_IDLE;
			break;
		case ESC_BYTE:
			/* ESC ESC: still waiting for a command letter */
			break;
		default:
			/* unknown command: drop it together with the ESC that prefixed it */
			esc_state = ESC_IDLE;
			break;
		}
		break;

	case ESC_M_COLUMN:
		esc_column = byte;
		esc_state = ESC_M_ROW;
		break;

	case ESC_M_ROW: {
		size_t x = esc_column;
		size_t y = byte;

		if (x >= VGA_WIDTH) {
			x = VGA_WIDTH - 1;
		}
		if (y >= VGA_HEIGHT) {
			y = VGA_HEIGHT - 1;
		}

		terminal_column = x;
		terminal_row = y;
		esc_state = ESC_IDLE;
		terminal_update_cursor();
		break;
	}

	case ESC_A_ATTR:
		terminal_setcolor(byte);
		esc_state = ESC_IDLE;
		break;

	case ESC_V_ARG:
		/* 0 or '0' hides, anything else shows: raw 0/1 and the ASCII
		 * characters '0'/'1' both work */
		terminal_cursor_setvisible(byte != '0' && byte != 0);
		esc_state = ESC_IDLE;
		break;

	case ESC_W_ARG:
		terminal_scroll_wrap = (byte != '0' && byte != 0);
		esc_state = ESC_IDLE;
		break;

	case ESC_IDLE:
		break;
	}
}

void terminal_putchar(char c) {
	unsigned char uc = c;

	if (esc_state != ESC_IDLE) {
		terminal_escape(uc);
		return;
	}

	if (uc == ESC_BYTE) {
		esc_state = ESC_PREFIX;
		return;
	}

	if (c == '\n') {
		terminal_column = 0;
		terminal_advance_row();
	} else if (c == '\f') {
		terminal_clear();
	} else if (c == '\r') {
		terminal_column = 0;
	} else if (c == '\t') {
		size_t spaces = 4 - (terminal_column % 4);
		for (size_t i = 0; i < spaces; i++) {
			terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
			if (++terminal_column == VGA_WIDTH) {
				terminal_wrap_or_stick();
			}
		}
	} else if (c == '\b') {
		if (terminal_column > 0) {
			terminal_column--;
		} else if (terminal_row > 0) {
			terminal_row--;
			terminal_column = VGA_WIDTH - 1;
		}
	} else if (uc < 0x20 || uc == 0x7F) {
	} else {
		terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
		if (++terminal_column == VGA_WIDTH) {
			terminal_wrap_or_stick();
		}
	}

	terminal_update_cursor();
}

void terminal_write(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}

void terminal_scroll(void) {
	for (size_t y = 1; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t from_index = y * VGA_WIDTH + x;
			const size_t to_index = (y - 1) * VGA_WIDTH + x;
			terminal_buffer[to_index] = terminal_buffer[from_index];
		}
	}
	for (size_t x = 0; x < VGA_WIDTH; x++) {
		const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
		terminal_buffer[index] = vga_entry(' ', terminal_color);
	}
	terminal_row = VGA_HEIGHT - 1;
}

void terminal_fsetcolor(enum ansi_color fg_color, enum ansi_color bg_color) {
	terminal_color = vga_entry_color((enum vga_color)fg_color, (enum vga_color)bg_color);
}

void terminal_clear(void) {
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}

	terminal_row = 0;
	terminal_column = 0;
	terminal_update_cursor();
}