/* SPDX-License-Identifier: BSD-3-Clause */

#include "eval.h"

#include <limits.h>
#include <stdbool.h>

#define CALC_MAX_DEPTH 64

typedef struct calc_parser {
	const char* p;
	int depth;
	int err;
} calc_parser_t;

static void skip_ws(calc_parser_t* ps) {
	while (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r') {
		ps->p++;
	}
}

static long parse_expr(calc_parser_t* ps);

static long parse_number(calc_parser_t* ps) {
	if (*ps->p < '0' || *ps->p > '9') {
		ps->err = CALC_ERR_SYNTAX;
		return 0;
	}

	long v = 0;

	while (*ps->p >= '0' && *ps->p <= '9') {
		int d = *ps->p - '0';

		if (v > (LONG_MAX - d) / 10) {
			ps->err = CALC_ERR_OVERFLOW;
			return 0;
		}

		v = v * 10 + d;
		ps->p++;
	}

	return v;
}

static long parse_factor(calc_parser_t* ps) {
	skip_ws(ps);

	if (ps->err) {
		return 0;
	}

	if (ps->depth >= CALC_MAX_DEPTH) {
		ps->err = CALC_ERR_COMPLEX;
		return 0;
	}

	char c = *ps->p;

	if (c == '(') {
		ps->p++;
		ps->depth++;
		
		long v = parse_expr(ps);
		ps->depth--;

		if (ps->err) {
			return 0;
		}

		skip_ws(ps);
		if (*ps->p != ')') {
			ps->err = CALC_ERR_SYNTAX;
			return 0;
		}

		ps->p++;
		return v;
	}

	if (c == '-' || c == '+') {
		ps->p++;
		ps->depth++;
		
		long v = parse_factor(ps);
		ps->depth--;

		if (ps->err) {
			return 0;
		}

		if (c == '+') {
			return v;
		}

		long r;
		if (__builtin_sub_overflow(0L, v, &r)) {
			ps->err = CALC_ERR_OVERFLOW;
			return 0;
		}
		
		return r;
	}

	return parse_number(ps);
}

static long parse_term(calc_parser_t* ps) {
	long v = parse_factor(ps);

	while (!ps->err) {
		skip_ws(ps);
		char c = *ps->p;

		if (c != '*' && c != '/' && c != '%') {
			break;
		}

		ps->p++;
		long rhs = parse_factor(ps);
		if (ps->err) {
			return 0;
		}

		if (c == '*') {
			long r;
			if (__builtin_mul_overflow(v, rhs, &r)) {
				ps->err = CALC_ERR_OVERFLOW;
				return 0;
			}
			
			v = r;
			continue;
		}

		if (rhs == 0) {
			ps->err = CALC_ERR_DIVZERO;
			return 0;
		}

		if (rhs == -1 && v == LONG_MIN) {
			ps->err = CALC_ERR_OVERFLOW;
			return 0;
		}

		v = (c == '/') ? v / rhs : v % rhs;
	}

	return v;
}

static long parse_expr(calc_parser_t* ps) {
	long v = parse_term(ps);

	while (!ps->err) {
		skip_ws(ps);
		char c = *ps->p;

		if (c != '+' && c != '-') {
			break;
		}

		ps->p++;
		long rhs = parse_term(ps);
		if (ps->err) {
			return 0;
		}

		long r;
		bool bad = (c == '+') ? __builtin_add_overflow(v, rhs, &r) : __builtin_sub_overflow(v, rhs, &r);
		if (bad) {
			ps->err = CALC_ERR_OVERFLOW;
			return 0;
		}
		
		v = r;
	}

	return v;
}

int calc_eval(const char* expr, long* out) {
	if (!expr) {
		return CALC_ERR_SYNTAX;
	}

	calc_parser_t ps = {
		.p = expr,
		.depth = 0,
		.err = CALC_OK
	};

	long v = parse_expr(&ps);
	if (ps.err) {
		return ps.err;
	}

	skip_ws(&ps);
	if (*ps.p != '\0') {
		return CALC_ERR_TRAILING;
	}

	*out = v;
	return CALC_OK;
}

const char* calc_eval_strerror(int code) {
	switch (code) {
		case CALC_OK:
			return "ok";
		case CALC_ERR_SYNTAX:
			return "malformed expression";
		case CALC_ERR_DIVZERO:
			return "division by zero";
		case CALC_ERR_OVERFLOW:
			return "integer overflow";
		case CALC_ERR_COMPLEX:
			return "expression is nested too deeply";
		case CALC_ERR_TRAILING:
			return "trailing characters after the expression";
		default:
			return "unknown error";
	}
}
