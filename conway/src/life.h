/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _CONWAY_LIFE_H
#define _CONWAY_LIFE_H

#include <stdbool.h>
#include <stddef.h>

#define LIFE_W 80
#define LIFE_H 24
#define LIFE_CELLS (LIFE_W * LIFE_H)

bool life_is_alive(char c);

int life_count(const unsigned char* board);

int life_step(const unsigned char* board, unsigned char* next);

void life_parse(const char* data, size_t len, unsigned char* board);

void life_format(const unsigned char* board, char* out);

#endif
