#include "kernel/handle/inode_type.h"
#include <kernel/kbd/core.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <panuti/kbd.h>
#include <string.h>
#include <kernel/kbd/dvc.h>

#define KBD_LINE_MAX 256

static char line_buffer[KBD_LINE_MAX];
static size_t line_len = 0;

static char keycode_to_char(keycode_t kc, bool shift, bool caps) {
	if (kc >= KEYCODE_A && kc <= KEYCODE_Z) {
		bool upper = caps ^ shift;
		char base = 'a' + (kc - KEYCODE_A);
		return upper ? (char)(base - 'a' + 'A') : base;
	}

	static const char digit_unshifted[] = "0123456789";
	static const char digit_shifted[]   = ")!@#$%^&*(";
	
	if (kc >= KEYCODE_0 && kc <= KEYCODE_9) {
		int idx = kc - KEYCODE_0;
		return shift ? digit_shifted[idx] : digit_unshifted[idx];
	}

	switch (kc) {
		case KEYCODE_SPACE:
			return ' ';
		case KEYCODE_PERIOD:
			return shift ? '>' : '.';
		case KEYCODE_COMMA:
			return shift ? '<' : ',';
		case KEYCODE_SEMICOLON:
			return shift ? ':' : ';';
		case KEYCODE_APOSTROPHE:
			return shift ? '"' : '\'';
		case KEYCODE_MINUS:
			return shift ? '_' : '-';
		case KEYCODE_EQUALS:
			return shift ? '+' : '=';
		case KEYCODE_SLASH:
			return shift ? '?' : '/';
		case KEYCODE_BACKSLASH:
			return shift ? '|' : '\\';
		case KEYCODE_LBRACKET:
			return shift ? '{' : '[';
		case KEYCODE_RBRACKET:
			return shift ? '}' : ']';
		case KEYCODE_BACKTICK:
			return shift ? '~' : '`';
		default:
			return 0; 
	}
}

static void console_putchar(char c) {
	handle_t console;
	if (handle_build("/dvc/console", &console) == 0) {
		console.ops->write(console.impl, &c, 1);
	}
}

static void console_erase_last(void) {
	console_putchar('\b');
	console_putchar(' ');
	console_putchar('\b');
}

static int kbd_line_read(void* impl, void* buf, size_t len) {
	(void)impl;

	while (1) {
		keypacket_t pkt;
		if (kbd_core_read_line_queue(&pkt) != 0) {
			return -1;
		}

		if (!pkt.pressed) {
			continue;
		}

		if (pkt.keycode == KEYCODE_ENTER) {
			console_putchar('\n');

			size_t n = line_len;
			if (n > len) {
				n = len; // truncate if caller's buffer is smaller than the line
			}
			
			memcpy(buf, line_buffer, n);
			line_len = 0;
			
			return (int)n;
		}

		if (pkt.keycode == KEYCODE_BACKSPACE) {
			if (line_len > 0) {
				line_len--;
				console_erase_last();
			}
			
			continue;
		}

		char c = keycode_to_char(pkt.keycode, pkt.shift, pkt.caps_lock);
		if (c == 0) {
			continue;
		}

		if (line_len < KBD_LINE_MAX - 1) {
			line_buffer[line_len++] = c;
			console_putchar(c);
		}
		
		// buffer full: silently drop further characters until Enter
	}
}

static const handle_ops_t kbd_line_ops = {
	.read = kbd_line_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.close = op_not_supported_close,
};

void kbd_line_init(void) {
	line_len = 0;
	registry_add("/dvc/kbd/line", INODE_FILE, nullptr, &kbd_line_ops);
}