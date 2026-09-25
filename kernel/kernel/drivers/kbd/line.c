/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "kernel/handle/inode_type.h"
#include <kernel/kbd/core.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/irq.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <panuti/kbd.h>
#include <panuti/errno.h>
#include <string.h>
#include <kernel/kbd/dvc.h>

#define KBD_LINE_MAX 256
#define KBD_LINE_CAP (KBD_LINE_MAX - 1)

typedef struct {
	char line_buffer[KBD_LINE_MAX];
	size_t line_len;
	size_t line_pos;
	bool in_use;
	task_t* owner;
} kbd_line_state_t;

static kbd_line_state_t line_state;
static handle_t console_handle;
static bool console_ready;

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
	if (!console_ready) {
		console_ready = handle_build("/dvc/console", &console_handle);
		if (!console_ready) {
			return;
		}
	}

	console_handle.ops->write(console_handle.impl, &c, 1);
}

static void console_erase_last(void) {
	console_putchar('\b');
	console_putchar(' ');
	console_putchar('\b');
}

static void console_bell(void) {
	console_putchar('\a');
}

static void clear_line(void) {
	while (line_state.line_len > 0) {
		console_erase_last();
		line_state.line_len--;
	}
	line_state.line_pos = 0;
}

static void backspace(void) {
	if (line_state.line_len == 0) {
		return;
	}

	if (line_state.line_buffer[line_state.line_len - 1] == '\t') {
		for (int i = 0; i < 4; i++) {
			console_erase_last();
		}
	} else {
		console_erase_last();
	}

	line_state.line_len--;
	if (line_state.line_pos > line_state.line_len) {
		line_state.line_pos = line_state.line_len;
	}
}

static int line_claim(void) {
	uint32_t flags = irq_save_disable();

	task_t* cur = sched_current();
	if (line_state.in_use && line_state.owner != cur) {
		irq_restore(flags);
		return PANUTIERRNO_BUSY;
	}

	bool fresh = !line_state.in_use;
	line_state.in_use = true;
	line_state.owner = cur;
	if (fresh) {
		line_state.line_len = 0;
		line_state.line_pos = 0;
	}

	irq_restore(flags);
	return 0;
}

static void line_release(void) {
	uint32_t flags = irq_save_disable();
	line_state.in_use = false;
	line_state.owner = nullptr;
	line_state.line_len = 0;
	line_state.line_pos = 0;
	irq_restore(flags);
}

static int deliver_line(void* buf, size_t len) {
	size_t avail = line_state.line_len - line_state.line_pos;
	size_t n = (avail < len - 1) ? avail : len - 1;

	memcpy(buf, line_state.line_buffer + line_state.line_pos, n);
	((char*)buf)[n] = '\0';
	line_state.line_pos += n;

	if (line_state.line_pos == line_state.line_len) {
		line_release();
	}

	return (int)n;
}

static int kbd_line_read(void* impl, void* buf, size_t len) {
	(void)impl;

	if (len == 0) {
		return 0;
	}

	int rc = line_claim();
	if (rc != 0) {
		return rc;
	}

	while (1) {
		keypacket_t pkt;
		if (kbd_core_read_line_queue(&pkt) != 0) {
			// a broken queue must not wedge the line lane
			line_release();
			return -1;
		}

		if (!pkt.pressed) {
			continue;
		}

		if (pkt.ctrl) {
			switch (pkt.keycode) {
			case KEYCODE_C:
			case KEYCODE_U:
				clear_line();
				continue;
			case KEYCODE_D:
				if (line_state.line_len == 0) {
					((char*)buf)[0] = '\0';
					line_release();
					return 0;
				}
				clear_line();
				continue;
			case KEYCODE_H:
				backspace();
				continue;
			default:
				continue;
			}
		}

		if (pkt.keycode == KEYCODE_BACKSPACE) {
			backspace();
			continue;
		}

		if (pkt.keycode == KEYCODE_ENTER) {
			console_putchar('\n');
			return deliver_line(buf, len);
		}

		if (pkt.keycode == KEYCODE_TAB) {
			if (line_state.line_len < KBD_LINE_CAP) {
				line_state.line_buffer[line_state.line_len++] = '\t';
				for (int i = 0; i < 4; i++) {
					console_putchar(' ');
				}
			} else {
				console_bell();
			}

			continue;
		}

		char c = keycode_to_char(pkt.keycode, pkt.shift, pkt.caps_lock);
		if (c == 0) {
			continue;
		}

		if (line_state.line_len < KBD_LINE_CAP) {
			line_state.line_buffer[line_state.line_len++] = c;
			console_putchar(c);
		} else {
			console_bell();
		}
	}
}

static int kbd_line_close(void* impl, struct task* self) {
	(void)impl;

	uint32_t flags = irq_save_disable();
	if (line_state.in_use && line_state.owner == self) {
		line_state.in_use = false;
		line_state.owner = nullptr;
		line_state.line_len = 0;
		line_state.line_pos = 0;
	}
	irq_restore(flags);

	return 0;
}

static const handle_ops_t kbd_line_ops = {
	.read = kbd_line_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.close = kbd_line_close,
};

void kbd_line_init(void) {
	line_state = (kbd_line_state_t){0};
	console_ready = false;
	registry_add("/dvc/kbd/line", INODE_FILE, nullptr, &kbd_line_ops);
}