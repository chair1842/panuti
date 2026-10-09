/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ps2.h"
#include "../../io.h"
#include <stdint.h>
#include "../../intpt/handlers/main.h"
#include <kernel/kbd/core.h>
#include <panuti/kbd.h>

#define PS2_DATA_PORT 0x60
#define PS2_IRQ_VECTOR 33 // IRQ1, per your PIC remap (master base 0x20 + 1)

#define PS2_SCAN_EXTENDED 0xE0 // marks the "enhanced" keys on scancode set 1
#define PS2_SCAN_PAUSE 0xE1    // introduces the pause/break burst

static const keycode_t scancode_table[128] = {
	[0x01] = KEYCODE_ESCAPE,
	[0x02] = KEYCODE_1, [0x03] = KEYCODE_2, [0x04] = KEYCODE_3, [0x05] = KEYCODE_4, [0x06] = KEYCODE_5,
	[0x07] = KEYCODE_6, [0x08] = KEYCODE_7, [0x09] = KEYCODE_8, [0x0A] = KEYCODE_9, [0x0B] = KEYCODE_0,
	[0x0C] = KEYCODE_MINUS, [0x0D] = KEYCODE_EQUALS, [0x0E] = KEYCODE_BACKSPACE, [0x0F] = KEYCODE_TAB,
	[0x10] = KEYCODE_Q, [0x11] = KEYCODE_W, [0x12] = KEYCODE_E, [0x13] = KEYCODE_R, [0x14] = KEYCODE_T,
	[0x15] = KEYCODE_Y, [0x16] = KEYCODE_U, [0x17] = KEYCODE_I, [0x18] = KEYCODE_O, [0x19] = KEYCODE_P,
	[0x1A] = KEYCODE_LBRACKET, [0x1B] = KEYCODE_RBRACKET, [0x1C] = KEYCODE_ENTER, [0x1D] = KEYCODE_LCTRL,
	[0x1E] = KEYCODE_A, [0x1F] = KEYCODE_S, [0x20] = KEYCODE_D, [0x21] = KEYCODE_F, [0x22] = KEYCODE_G,
	[0x23] = KEYCODE_H, [0x24] = KEYCODE_J, [0x25] = KEYCODE_K, [0x26] = KEYCODE_L,
	[0x27] = KEYCODE_SEMICOLON, [0x28] = KEYCODE_APOSTROPHE, [0x29] = KEYCODE_NONE,
	[0x2A] = KEYCODE_LSHIFT, [0x2B] = KEYCODE_BACKSLASH,
	[0x2C] = KEYCODE_Z, [0x2D] = KEYCODE_X, [0x2E] = KEYCODE_C, [0x2F] = KEYCODE_V, [0x30] = KEYCODE_B,
	[0x31] = KEYCODE_N, [0x32] = KEYCODE_M, [0x33] = KEYCODE_COMMA, [0x34] = KEYCODE_PERIOD, 
	[0x35] = KEYCODE_SLASH, [0x36] = KEYCODE_RSHIFT,
	[0x37] = KEYCODE_NONE, [0x38] = KEYCODE_LALT, [0x39] = KEYCODE_SPACE, [0x3A] = KEYCODE_CAPS_LOCK,
	[0x3B] = KEYCODE_F1, [0x3C] = KEYCODE_F2, [0x3D] = KEYCODE_F3, [0x3E] = KEYCODE_F4,
	[0x3F] = KEYCODE_F5, [0x40] = KEYCODE_F6, [0x41] = KEYCODE_F7, [0x42] = KEYCODE_F8,
	[0x43] = KEYCODE_F9, [0x44] = KEYCODE_F10,
	[0x45] = KEYCODE_NUM_LOCK, [0x46] = KEYCODE_SCROLL_LOCK,
	[0x47] = KEYCODE_HOME, [0x48] = KEYCODE_UP, [0x49] = KEYCODE_PAGE_UP, [0x4A] = KEYCODE_NONE,
	[0x4B] = KEYCODE_LEFT, [0x4C] = KEYCODE_NONE, [0x4D] = KEYCODE_RIGHT, [0x4E] = KEYCODE_NONE,
	[0x4F] = KEYCODE_END, [0x50] = KEYCODE_DOWN, [0x51] = KEYCODE_PAGE_DOWN,
	[0x52] = KEYCODE_INSERT, [0x53] = KEYCODE_DELETE,
	[0x57] = KEYCODE_F11, [0x58] = KEYCODE_F12,
};

static const keycode_t extended_scancode_table[128] = {
	[0x1C] = KEYCODE_ENTER,
	[0x1D] = KEYCODE_RCTRL,
	[0x35] = KEYCODE_SLASH,
	[0x38] = KEYCODE_RALT,
	[0x47] = KEYCODE_HOME,
	[0x48] = KEYCODE_UP,
	[0x49] = KEYCODE_PAGE_UP,
	[0x4B] = KEYCODE_LEFT,
	[0x4D] = KEYCODE_RIGHT,
	[0x4F] = KEYCODE_END,
	[0x50] = KEYCODE_DOWN,
	[0x51] = KEYCODE_PAGE_DOWN,
	[0x52] = KEYCODE_INSERT,
	[0x53] = KEYCODE_DELETE,
};

static bool extended = false;
static uint8_t pause_bytes_left = 0;

static bool shift_held = false;
static bool ctrl_held = false;
static bool alt_held = false;
static bool caps_lock_on = false;
static bool scroll_lock_on = false;
static bool num_lock_on = false;

static void ps2_irq1_handler(registers_t* regs) {
	(void)regs;

	uint8_t scancode = inb(PS2_DATA_PORT);

	if (pause_bytes_left > 0) {
		pause_bytes_left--;
		return;
	}

	if (scancode == PS2_SCAN_PAUSE) {
		extended = false;
		pause_bytes_left = 5;
		return;
	}

	if (scancode == PS2_SCAN_EXTENDED) {
		extended = true;
		return;
	}

	bool pressed = !(scancode & 0x80);
	uint8_t index = scancode & 0x7F;
	bool is_extended = extended;
	extended = false;
	keycode_t kc = is_extended ? extended_scancode_table[index] : scancode_table[index];

	if (kc == KEYCODE_NONE) {
		return;
	}

	if (kc == KEYCODE_LSHIFT || kc == KEYCODE_RSHIFT) {
		shift_held = pressed;
	} else if (kc == KEYCODE_LCTRL || kc == KEYCODE_RCTRL) {
		ctrl_held = pressed;
	} else if (kc == KEYCODE_LALT || kc == KEYCODE_RALT) {
		alt_held = pressed;
	} else if (kc == KEYCODE_CAPS_LOCK && pressed) {
		caps_lock_on = !caps_lock_on;
	} else if (kc == KEYCODE_SCROLL_LOCK && pressed) {
		scroll_lock_on = !scroll_lock_on;
	} else if (kc == KEYCODE_NUM_LOCK && pressed) {
		num_lock_on = !num_lock_on;
	}

	keypacket_t pkt = {
		.keycode = kc,
		.pressed = pressed,
		.shift = shift_held,
		.ctrl = ctrl_held,
		.alt = alt_held,
		.caps_lock = caps_lock_on,
		.scroll_lock = scroll_lock_on,
		.num_lock = num_lock_on,
	};
	
	kbd_core_push(pkt);
}

void kbd_ps2_init(void) {
	extended = false;
	pause_bytes_left = 0;
	shift_held = false;
	ctrl_held = false;
	alt_held = false;
	caps_lock_on = false;
	scroll_lock_on = false;
	num_lock_on = false;

	register_handler(PS2_IRQ_VECTOR, ps2_irq1_handler);
}