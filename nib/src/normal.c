/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "editor.h"

static bool is_modifier(keycode_t kc) {
	switch (kc) {
		case KEYCODE_LSHIFT:
		case KEYCODE_RSHIFT:
		case KEYCODE_LALT:
		case KEYCODE_RALT:
		case KEYCODE_LCTRL:
		case KEYCODE_RCTRL:
		case KEYCODE_CAPS_LOCK:
		case KEYCODE_SCROLL_LOCK:
		case KEYCODE_NUM_LOCK:
			return true;
		default:
			return false;
	}
}

static bool is_motion(char c) {
	switch (c) {
		case 'h':
		case 'j':
		case 'k':
		case 'l':
		case 'w':
		case 'b':
		case 'e':
		case '0':
		case '^':
		case '$':
			return true;
		default:
			return false;
	}
}

static bool is_operator(char c) {
	switch (c) {
		case 'd':
		case 'c':
			return true;
		default:
			return false;
	}
}

static void reset_counts(void) {
	EGG.nmstate.count = 0;
	EGG.nmstate.operator_count = 0;
}

static bool has_count(void) {
	return EGG.nmstate.count > 0 || EGG.nmstate.operator_count > 0;
}

static int combined_count(void) {
	int before = EGG.nmstate.count > 0 ? EGG.nmstate.count : 1;
	int after = EGG.nmstate.operator_count > 0 ? EGG.nmstate.operator_count : 1;

	if (before > INT_MAX / after) {
		return INT_MAX;
	}

	return before * after;
}

static int take_count(void) {
	int count = combined_count();
	reset_counts();
	return count;
}

static bool append_count(int* count, int digit) {
	if (*count > (INT_MAX - digit) / 10) {
		*count = INT_MAX;
		return false;
	}

	*count = *count * 10 + digit;
	return true;
}

static bool is_word_char(char c) {
	return c != ' ' && c != '\t';
}

static bool find_next_word(int* y, int* x) {
	int cy = *y;
	int cx = *x;
	line_t* l = &EGG.lines[cy];

	if (cx < l->len && is_word_char(l->string[cx])) {
		while (cx < l->len && is_word_char(l->string[cx])) {
			cx++;
		}
	}

	while (cy < EGG.nlines) {
		l = &EGG.lines[cy];

		while (cx < l->len && !is_word_char(l->string[cx])) {
			cx++;
		}

		if (cx < l->len) {
			*y = cy;
			*x = cx;
			return true;
		}

		cy++;
		cx = 0;
	}

	return false;
}

static bool find_previous_word(int* y, int* x) {
	int cy = *y;
	int cx = *x;

	while (cy >= 0) {
		line_t* l = &EGG.lines[cy];

		if (cx == 0) {
			if (cy == 0) {
				return false;
			}

			cy--;
			cx = EGG.lines[cy].len;
			continue;
		}

		cx--;

		if (cx < l->len && is_word_char(l->string[cx])) {
			while (cx > 0 && is_word_char(l->string[cx - 1])) {
				cx--;
			}

			*y = cy;
			*x = cx;
			return true;
		}
	}

	return false;
}

static bool find_word_end(int* y, int* x) {
	int cy = *y;
	int cx = *x;

	if (cy < EGG.nlines && cx < EGG.lines[cy].len && is_word_char(EGG.lines[cy].string[cx])) {
		line_t* l = &EGG.lines[cy];

		if (cx + 1 < l->len && is_word_char(l->string[cx + 1])) {
			while (cx + 1 < l->len && is_word_char(l->string[cx + 1])) {
				cx++;
			}

			*y = cy;
			*x = cx;
			return true;
		}

		cx++;
	}

	while (cy < EGG.nlines) {
		line_t* l = &EGG.lines[cy];

		while (cx < l->len && !is_word_char(l->string[cx])) {
			cx++;
		}

		if (cx < l->len) {
			while (cx + 1 < l->len && is_word_char(l->string[cx + 1])) {
				cx++;
			}

			*y = cy;
			*x = cx;
			return true;
		}

		cy++;
		cx = 0;
	}

	return false;
}

static bool move_one(char c, int* y, int* x, bool* linewise, bool* inclusive) {
	switch (c) {
		case 'h':
			if (*x == 0) {
				return false;
			}

			(*x)--;
			break;
		case 'l':
			if (*x >= EGG.lines[*y].len) {
				return false;
			}

			(*x)++;
			break;
		case 'j':
			if (*y + 1 >= EGG.nlines) {
				return false;
			}

			(*y)++;
			*linewise = true;
			break;
		case 'k':
			if (*y == 0) {
				return false;
			}

			(*y)--;
			*linewise = true;
			break;
		case '0':
			*x = 0;
			break;
		case '^':
			*x = 0;
			while (*x < EGG.lines[*y].len &&
				(EGG.lines[*y].string[*x] == ' ' || EGG.lines[*y].string[*x] == '\t')) {
				(*x)++;
			}
			
			break;
		case '$':
			*x = EGG.lines[*y].len;
			break;
		case 'w':
			if (!find_next_word(y, x)) {
				return false;
			}
			
			break;
		case 'b':
			if (!find_previous_word(y, x)) {
				return false;
			}
			
			break;
		case 'e':
			if (!find_word_end(y, x)) {
				return false;
			}

			*inclusive = true;
			break;
		default:
			return false;
	}

	return true;
}

static bool resolve_motion(char c, int count, motion_t* m) {
	int y = EGG.cur_y;
	int x = EGG.cur_x;
	bool linewise = false;
	bool inclusive = false;
	bool moved = false;

	if (count < 1) {
		count = 1;
	}

	if ((c == '$' || c == '^') && count > 1) {
		for (int i = 1; i < count; i++) {
			if (y + 1 >= EGG.nlines) {
				break;
			}

			y++;
		}

		x = 0;
		if (c == '^') {
			while (x < EGG.lines[y].len &&
				(EGG.lines[y].string[x] == ' ' || EGG.lines[y].string[x] == '\t')) {
				x++;
			}
		} else {
			x = EGG.lines[y].len;
		}

		moved = true;
	} else {
		for (int i = 0; i < count; i++) {
			if (!move_one(c, &y, &x, &linewise, &inclusive)) {
				break;
			}

			moved = true;
		}
	}

	if (!moved) {
		return false;
	}

	m->y = y;
	m->x = x;
	m->linewise = linewise;
	m->inclusive = inclusive;
	return true;
}

static bool motion_range(char c, int count, range_t* r) {
	motion_t m;

	if (!resolve_motion(c, count, &m)) {
		return false;
	}

	if (m.linewise) {
		r->linewise = true;
		r->y1 = EGG.cur_y < m.y ? EGG.cur_y : m.y;
		r->x1 = 0;
		r->y2 = EGG.cur_y < m.y ? m.y : EGG.cur_y;
		r->x2 = 0;
		return true;
	}

	r->linewise = false;

	if (m.y < EGG.cur_y || (m.y == EGG.cur_y && m.x < EGG.cur_x)) {
		r->y1 = m.y;
		r->x1 = m.x;
		r->y2 = EGG.cur_y;
		r->x2 = EGG.cur_x;
	} else {
		r->y1 = EGG.cur_y;
		r->x1 = EGG.cur_x;
		r->y2 = m.y;
		r->x2 = m.x + (m.inclusive ? 1 : 0);

		if (r->x2 > EGG.lines[r->y2].len) {
			r->x2 = EGG.lines[r->y2].len;
		}
	}

	return true;
}

static int clamp(int v, int lo, int hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

static bool delete_range(range_t* r) {
	if (r->linewise) {
		if (r->y1 == 0 && r->y2 >= EGG.nlines - 1) {
			if (!line_set(&EGG.lines[0], "", 0)) {
				return false;
			}

			for (int y = EGG.nlines - 1; y >= 1; y--) {
				line_delete_at(y);
			}

			EGG.cur_y = 0;
			EGG.cur_x = 0;
			return true;
		}

		for (int y = r->y2; y >= r->y1; y--) {
			line_delete_at(y);
		}

		EGG.cur_y = r->y1 < EGG.nlines ? r->y1 : EGG.nlines - 1;
		EGG.cur_x = 0;
		return true;
	}

	line_t* first = &EGG.lines[r->y1];
	line_t* last = &EGG.lines[r->y2];
	int x1 = clamp(r->x1, 0, first->len);
	int x2 = clamp(r->x2, 0, last->len);

	if (r->y1 == r->y2) {
		if (x1 < x2) {
			memmove(first->string + x1, first->string + x2, (size_t)(first->len - x2));
			first->len -= x2 - x1;
			first->string[first->len] = '\0';
		}

		EGG.cur_y = r->y1;
		EGG.cur_x = x1;
		return true;
	}

	int rem = last->len - x2;
	if (rem > LINE_LEN_MAX - x1) {
		return false;
	}

	if (!line_reserve(first, x1 + rem + 1)) {
		return false;
	}

	if (rem > 0) {
		memmove(first->string + x1, last->string + x2, (size_t)rem);
	}

	first->len = x1 + rem;
	first->string[first->len] = '\0';

	for (int y = r->y2; y > r->y1; y--) {
		line_delete_at(y);
	}

	EGG.cur_y = r->y1;
	EGG.cur_x = x1;
	return true;
}

static int execute_operator(char op, range_t* r) {
	if (op == 'c' && r->linewise) {
		if (!line_set(&EGG.lines[r->y1], "", 0)) {
			set_errmsg("out of memory");
			return 2;
		}

		for (int y = r->y2; y > r->y1; y--) {
			line_delete_at(y);
		}

		EGG.cur_y = r->y1;
		EGG.cur_x = 0;
	} else if (!delete_range(r)) {
		set_errmsg("out of memory");
		return 2;
	}

	EGG.dirty = true;

	if (op == 'c') {
		EGG.insert_x = EGG.cur_x;
		EGG.mode = EM_INSERT;
	}

	return 0;
}

static int join(int count) {
	if (count < 1) {
		count = 1;
	}

	for (int i = 0; i < count && EGG.cur_y + 1 < EGG.nlines; i++) {
		line_t* cur = &EGG.lines[EGG.cur_y];
		line_t* next = &EGG.lines[EGG.cur_y + 1];

		int src = 0;
		while (src < next->len && (next->string[src] == ' ' || next->string[src] == '\t')) {
			src++;
		}

		bool space = cur->len > 0 
			&& src < next->len 
			&& cur->string[cur->len - 1] != ' ' 
			&& cur->string[cur->len - 1] != '\t';

		int add = next->len - src + (space ? 1 : 0);
		if (add > LINE_LEN_MAX - cur->len) {
			set_errmsg("line too long");
			return 2;
		}

		if (!line_reserve(cur, cur->len + add + 1)) {
			set_errmsg("out of memory");
			return 2;
		}

		int at = cur->len;

		if (space) {
			cur->string[cur->len++] = ' ';
		}

		if (src < next->len) {
			memcpy(cur->string + cur->len, next->string + src, (size_t)(next->len - src));
			cur->len += next->len - src;
		}

		cur->string[cur->len] = '\0';

		line_delete_at(EGG.cur_y + 1);
		EGG.cur_x = at;
		EGG.dirty = true;
	}

	return 0;
}

static int operator_to_eol(char op, int count) {
	int last = EGG.cur_y;
	if (count > EGG.nlines - EGG.cur_y) {
		last = EGG.nlines - 1;
	} else {
		last = EGG.cur_y + count - 1;
	}

	range_t r = {
		.y1 = EGG.cur_y,
		.y2 = last,
		.x1 = EGG.cur_x,
		.x2 = EGG.lines[last].len,
		.linewise = false,
	};

	return execute_operator(op, &r);
}

static int process_operator(keypacket_t kp) {
	char c = keypacket_to_ascii(kp);
	char op = EGG.nmstate.pending_op;
	bool had_count = has_count();
	int count = combined_count();

	if (kp.keycode == KEYCODE_ESCAPE) {
		EGG.nmstate.pending_op = 0;
		reset_counts();
		return 0;
	}

	range_t r;
	bool have_range = false;

	if (c == 'g') {
		EGG.nmstate.pending_g = true;
		return 0;
	} else if (c == 'G') {
		int target = had_count ? clamp(count - 1, 0, EGG.nlines - 1) : EGG.nlines - 1;
		r.linewise = true;
		r.y1 = EGG.cur_y < target ? EGG.cur_y : target;
		r.y2 = EGG.cur_y > target ? EGG.cur_y : target;
		r.x1 = 0;
		r.x2 = 0;
		have_range = true;
	} else if (c == op) {
		int last = count > EGG.nlines - EGG.cur_y
			? EGG.nlines - 1
			: EGG.cur_y + count - 1;
		r.linewise = true;
		r.y1 = EGG.cur_y;
		r.x1 = 0;
		r.y2 = last;
		r.x2 = 0;
		have_range = true;
	} else if (is_motion(c)) {
		have_range = motion_range(c, count, &r);
	}

	EGG.nmstate.pending_op = 0;
	reset_counts();

	if (!have_range) {
		return 0;
	}

	return execute_operator(op, &r);
}

int handin_normal(keypacket_t kp) {
	if (is_modifier(kp.keycode)) {
		return 0;
	}

	char ascc = keypacket_to_ascii(kp);

	if (EGG.nmstate.pending_replace) {
		EGG.nmstate.pending_replace = false;

		if (kp.keycode == KEYCODE_ESCAPE || ascc == 0) {
			reset_counts();
			return 0;
		}

		int count = take_count();
		int len = EGG.lines[EGG.cur_y].len;
		bool changed = false;

		for (int i = 0; i < count && EGG.cur_x + i < len; i++) {
			EGG.lines[EGG.cur_y].string[EGG.cur_x + i] = ascc;
			changed = true;
		}

		if (changed) {
			EGG.dirty = true;
		}

		return 0;
	}

	if (ascc >= '0' && ascc <= '9') {
		int* target = EGG.nmstate.pending_op
			? &EGG.nmstate.operator_count
			: &EGG.nmstate.count;

		if (ascc != '0' || *target > 0) {
			append_count(target, ascc - '0');
			return 0;
		}
	}

	if (kp.keycode == KEYCODE_ESCAPE) {
		EGG.nmstate.pending_op = 0;
		EGG.nmstate.pending_g = false;
		reset_counts();
		EGG.show_errmsg = false;
		return 0;
	}

	if (EGG.nmstate.pending_g) {
		EGG.nmstate.pending_g = false;

		if (ascc != 'g') {
			if (EGG.nmstate.pending_op) {
				return process_operator(kp);
			}

			reset_counts();
			return 0;
		}

		bool had_count = has_count();
		int count = combined_count();

		if (EGG.nmstate.pending_op) {
			char op = EGG.nmstate.pending_op;
			EGG.nmstate.pending_op = 0;

			int target = had_count ? clamp(count - 1, 0, EGG.nlines - 1) : 0;
			range_t r = {
				.y1 = EGG.cur_y < target ? EGG.cur_y : target,
				.y2 = EGG.cur_y > target ? EGG.cur_y : target,
				.x1 = 0,
				.x2 = 0,
				.linewise = true,
			};

			reset_counts();
			return execute_operator(op, &r);
		}

		goto_line(had_count ? clamp(count - 1, 0, EGG.nlines - 1) : 0);
		reset_counts();
		return 0;
	}

	if (EGG.nmstate.pending_op) {
		return process_operator(kp);
	}

	if (is_operator(ascc)) {
		EGG.nmstate.pending_op = ascc;
		return 0;
	}

	if (ascc == 'g') {
		EGG.nmstate.pending_g = true;
		return 0;
	}

	if (ascc == 'r') {
		EGG.nmstate.pending_replace = true;
		return 0;
	}

	bool had_count = has_count();
	int count = take_count();

	if (ascc == ':') {
		EGG.mode = EM_COMMAND;
		return 0;
	} else if (kp.keycode == KEYCODE_ENTER) {
		for (int i = 0; i < count; i++) {
			if (!line_new_at(EGG.cur_y + 1)) {
				set_errmsg("out of memory");
				return 2;
			}
		}

		EGG.dirty = true;
		EGG.cur_y++;
		EGG.cur_x = 0;

		return 0;
	} else if (ascc == 'o' || ascc == 'O') {
		int at = ascc == 'o' ? EGG.cur_y + 1 : EGG.cur_y;

		for (int i = 0; i < count; i++) {
			if (!line_new_at(at)) {
				set_errmsg("out of memory");
				return 2;
			}
		}

		EGG.dirty = true;
		if (ascc == 'o') {
			EGG.cur_y++;
		}

		EGG.cur_x = 0;
		EGG.insert_x = 0;
		EGG.mode = EM_INSERT;

		return 0;
	}

	if (is_motion(ascc)) {
		motion_t m;

		if (resolve_motion(ascc, count, &m)) {
			EGG.cur_y = m.y;
			EGG.cur_x = m.x;
		}

		return 0;
	}

	switch (ascc) {
		case 'G':
			goto_line(had_count ? clamp(count - 1, 0, EGG.nlines - 1) : EGG.nlines - 1);
			return 0;
		case 'i':
			EGG.insert_x = EGG.cur_x;
			EGG.mode = EM_INSERT;
			
			break;
		case 'a':
			if (EGG.cur_x < EGG.lines[EGG.cur_y].len) {
				EGG.cur_x++;
			}
			
			EGG.insert_x = EGG.cur_x;
			EGG.mode = EM_INSERT;
			
			break;
		case 'A':
			EGG.cur_x = EGG.lines[EGG.cur_y].len;
			EGG.insert_x = EGG.cur_x;
			EGG.mode = EM_INSERT;
			
			break;
		case 'x':
		case 'X': 
			{
				int len = EGG.lines[EGG.cur_y].len;
				int x1;
				int x2;

				if (ascc == 'x') {
					if (EGG.cur_x >= len) {
						break;
					}

					x1 = EGG.cur_x;
					x2 = count > len - EGG.cur_x ? len : EGG.cur_x + count;
				} else {
					if (EGG.cur_x == 0) {
						break;
					}

					x1 = count > EGG.cur_x ? 0 : EGG.cur_x - count;
					x2 = EGG.cur_x;
				}

				range_t r = {.y1 = EGG.cur_y, .y2 = EGG.cur_y, .x1 = x1, .x2 = x2, .linewise = false};

				return execute_operator('d', &r);
			}
		case 'D':
			return operator_to_eol('d', count);
		case 'C':
			return operator_to_eol('c', count);
		case 'J':
			return join(count);
		default:
			break;
	}

	return 0;
}
