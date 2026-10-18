/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

#include "life.h"

bool life_is_alive(char c) {
	switch (c) {
	case '#':
	case '*':
	case 'O':
	case 'o':
	case 'X':
	case 'x':
	case '1':
		return true;
	default:
		return false;
	}
}

int life_count(const unsigned char* board) {
	int alive = 0;

	for (int i = 0; i < LIFE_CELLS; i++) {
		alive += board[i] ? 1 : 0;
	}

	return alive;
}

static int neighbours(const unsigned char* board, int x, int y) {
	int count = 0;

	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			if (dx == 0 && dy == 0) {
				continue;
			}

			int nx = (x + dx + LIFE_W) % LIFE_W;
			int ny = (y + dy + LIFE_H) % LIFE_H;
			count += board[ny * LIFE_W + nx] ? 1 : 0;
		}
	}

	return count;
}

int life_step(const unsigned char* board, unsigned char* next) {
	int alive = 0;

	for (int y = 0; y < LIFE_H; y++) {
		for (int x = 0; x < LIFE_W; x++) {
			int n = neighbours(board, x, y);
			unsigned char live;

			if (board[y * LIFE_W + x]) {
				live = (n == 2 || n == 3);
			} else {
				live = (n == 3);
			}

			next[y * LIFE_W + x] = live;
			alive += live;
		}
	}

	return alive;
}

void life_parse(const char* data, size_t len, unsigned char* board) {
	memset(board, 0, LIFE_CELLS);

	int x = 0;
	int y = 0;

	for (size_t i = 0; i < len; i++) {
		char c = data[i];

		if (c == '\r') {
			continue;
		}

		if (c == '\n') {
			if (++y >= LIFE_H) {
				break;
			}

			x = 0;
			continue;
		}

		if (y < LIFE_H && x < LIFE_W) {
			board[y * LIFE_W + x] = life_is_alive(c) ? 1 : 0;
		}

		x++;
	}
}

void life_format(const unsigned char* board, char* out) {
	for (int y = 0; y < LIFE_H; y++) {
		for (int x = 0; x < LIFE_W; x++) {
			out[y * (LIFE_W + 1) + x] = board[y * LIFE_W + x] ? '#' : '.';
		}

		out[y * (LIFE_W + 1) + LIFE_W] = '\n';
	}
}
