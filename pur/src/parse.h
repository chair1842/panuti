/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef PUR_PARSE_H
#define PUR_PARSE_H

// The most expressions one command may chain together. Each expression is
// loaded and run on its own, so this only bounds how far a single line may
// drive the shell before it is told no.
#define PUR_MAX_EXPRS 64

typedef enum {
	PUR_PARSE_OK = 0,
	PUR_PARSE_EMPTY = 1,
	PUR_PARSE_ERR_SYNTAX,
	PUR_PARSE_ERR_EMPTY_GROUP,
	PUR_PARSE_ERR_QUOTE,
	PUR_PARSE_ERR_NODES,
	PUR_PARSE_ERR_LEAVES,
	PUR_PARSE_ERR_ARGV,
	PUR_PARSE_ERR_COMPLEX,
	PUR_PARSE_ERR_MEMORY,
	PUR_PARSE_ERR_OPERATOR,
	PUR_PARSE_ERR_TRAILING_BG,
	PUR_PARSE_ERR_RESERVED,
	PUR_PARSE_ERR_EXPRS,
} pur_parse_status_t;

// The command level operator that joins one expression to the next.
typedef enum {
	PUR_OP_NONE = 0,
	PUR_OP_AND,
	PUR_OP_OR,
	PUR_OP_BG,
} pur_op_t;

// Parse and validate a whole command: expressions chained by '&&', '||' or
// '&'. The entire command is checked before anything runs, so a later syntax
// error stops the earlier expressions too. On success the command is walked
// with pur_parse_nexprs / pur_parse_load / pur_parse_op.
int pur_parse_command(char *line);

// how many expressions the last pur_parse_command() found
int pur_parse_nexprs(void);

// load expression `index` into the global AST (reset, then parse). Only valid
// after a successful pur_parse_command().
int pur_parse_load(int index);

// the operator that joins expression `index` to the one after it.
pur_op_t pur_parse_op(int index);

// the text of an operator, "" for PUR_OP_NONE
const char *pur_parse_op_str(pur_op_t op);

// whether the expression after `op` should run, given the status so far
bool pur_op_should_run(pur_op_t op, int status);

int pur_leaf_argv(int leaf, char **argv);
const char *pur_parse_strerror(int status);

#endif
