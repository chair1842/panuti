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

void terminal_putchar(char c) {
	unsigned char uc = c;

	if (c == '\n') {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT) {
			terminal_scroll();
			terminal_row = VGA_HEIGHT - 1;
		}
	} else if (c == '\f') {
		terminal_clear();
		terminal_row = 0;
		terminal_column = 0;
	} else if (c == '\t') {
		size_t spaces = 4 - (terminal_column % 4);
		for (size_t i = 0; i < spaces; i++) {
			terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
			if (++terminal_column == VGA_WIDTH) {
				terminal_column = 0;
				if (++terminal_row == VGA_HEIGHT) {
					terminal_scroll();
					terminal_row = VGA_HEIGHT - 1;
				}
			}
		}
	} else if (c == '\b') {
		if (terminal_column > 0) {
			terminal_column--;
		} else if (terminal_row > 0) {
			terminal_row--;
			terminal_column = VGA_WIDTH - 1;
		}
	} else {
		terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
		if (++terminal_column == VGA_WIDTH) {
			terminal_column = 0;
			if (++terminal_row == VGA_HEIGHT) {
				terminal_scroll();
				terminal_row = VGA_HEIGHT - 1;
			}
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
}