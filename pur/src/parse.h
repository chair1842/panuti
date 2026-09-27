/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef PUR_PARSE_H
#define PUR_PARSE_H

#include <stdbool.h>

typedef enum {
	PUR_PARSE_OK = 0,
	PUR_PARSE_EMPTY = 1,
	PUR_PARSE_ERR_SYNTAX,
	PUR_PARSE_ERR_EMPTY_GROUP,
	PUR_PARSE_ERR_QUOTE,
	PUR_PARSE_ERR_TOKENS,
	PUR_PARSE_ERR_NODES,
	PUR_PARSE_ERR_LEAVES,
	PUR_PARSE_ERR_ARGV,
	PUR_PARSE_ERR_COMPLEX,
} pur_parse_status_t;

int pur_parse(char *line);
int pur_leaf_argv(int leaf, char **argv);
const char *pur_parse_strerror(int status);

#endif
