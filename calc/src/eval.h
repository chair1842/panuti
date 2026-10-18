/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef CALC_EVAL_H
#define CALC_EVAL_H

enum {
	CALC_OK = 0,
	CALC_ERR_SYNTAX = -1,
	CALC_ERR_DIVZERO = -2,
	CALC_ERR_OVERFLOW = -3,
	CALC_ERR_COMPLEX = -4,
	CALC_ERR_TRAILING = -5,
};

int calc_eval(const char* expr, long* out);

const char* calc_eval_strerror(int code);

#endif
