/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/kbd.h>

char keypacket_to_ascii(const keypacket_t keypacket) {
	keycode_t kc = keypacket.keycode;
	bool shift = keypacket.shift;
	bool caps = keypacket.caps_lock;

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