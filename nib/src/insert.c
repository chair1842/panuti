/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "editor.h"

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

				if (cur->len > LINE_LEN_MAX - prev->len) {
					set_errmsg("line too long");
					return 0;
				}

				if (!line_reserve(prev, prev->len + cur->len + 1)) {
					set_errmsg("out of memory");
					return 0;
				}

				if (cur->len > 0) {
					memcpy(prev->string + prev->len, cur->string, (size_t)cur->len);
				}

				prev->len += cur->len;
				prev->string[prev->len] = '\0';

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
