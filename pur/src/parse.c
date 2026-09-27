/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "parse.h"
#include "ast.h"

#include <stddef.h>

#define PUR_MAX_GROUP_MEMBERS 32

typedef enum {
	PUR_T_END = 0,
	PUR_T_WORD,
	PUR_T_LPAREN,
	PUR_T_RPAREN,
	PUR_T_SEMI,
	PUR_T_GT,
} pur_tok_t;

typedef struct {
	pur_tok_t type;
	int off;
	int len;
} pur_token_t;

static pur_token_t toks[PUR_MAX_TOKENS];
static int ntoks;
static int pos;
static char *line;

static bool is_space(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool is_delim(char c) {
	return is_space(c) || c == '(' || c == ')' || c == ';' || c == '>' || c == '"';
}

static int tokenize(void) {
	char *p = line;
	ntoks = 0;

	while (*p) {
		if (is_space(*p)) {
			p++;
			continue;
		}

		int type = PUR_T_END;
		int off;
		int len;

		if (*p == '(' || *p == ')' || *p == ';' || *p == '>') {
			if (*p == '(') {
				type = PUR_T_LPAREN;
			} else if (*p == ')') {
				type = PUR_T_RPAREN;
			} else if (*p == ';') {
				type = PUR_T_SEMI;
			} else {
				type = PUR_T_GT;
			}
			off = (int)(p - line);
			len = 1;
			p++;
		} else if (*p == '"') {
			type = PUR_T_WORD;
			p++;
			off = (int)(p - line);
			while (*p && *p != '"') {
				p++;
			}
			if (*p != '"') {
				return PUR_PARSE_ERR_QUOTE;
			}
			len = (int)(p - line) - off;
			p++;
		} else {
			type = PUR_T_WORD;
			off = (int)(p - line);
			while (*p && !is_delim(*p)) {
				p++;
			}
			len = (int)(p - line) - off;
		}

		if (ntoks >= PUR_MAX_TOKENS) {
			return PUR_PARSE_ERR_TOKENS;
		}

		toks[ntoks].type = (pur_tok_t)type;
		toks[ntoks].off = off;
		toks[ntoks].len = len;
		ntoks++;
	}

	if (ntoks >= PUR_MAX_TOKENS) {
		return PUR_PARSE_ERR_TOKENS;
	}

	toks[ntoks].type = PUR_T_END;
	toks[ntoks].off = 0;
	toks[ntoks].len = 0;
	return PUR_PARSE_OK;
}

static void terminate_words(void) {
	for (int i = 0; i < ntoks; i++) {
		if (toks[i].type == PUR_T_WORD) {
			line[toks[i].off + toks[i].len] = '\0';
		}
	}
}

static int parse_stmt(int depth, int *out);
static int parse_operand(int depth, int *out);
static int parse_group(int depth, int *out);

static int parse_leaf(int *out) {
	int argv_first = pur_nargv;
	int argc = 0;

	while (toks[pos].type == PUR_T_WORD) {
		if (argc >= PUR_MAX_ARGV) {
			return PUR_PARSE_ERR_ARGV;
		}
		if (pur_argv_push(toks[pos].off) < 0) {
			return PUR_PARSE_ERR_ARGV;
		}
		argc++;
		pos++;
	}

	int leaf = pur_leaf_new(argv_first, argc);
	if (leaf < 0) {
		return PUR_PARSE_ERR_LEAVES;
	}

	int n = pur_node_new(PUR_NODE_LEAF);
	if (n < 0) {
		return PUR_PARSE_ERR_NODES;
	}

	pur_nodes[n].first = leaf;
	*out = n;
	return PUR_PARSE_OK;
}

static int parse_operand(int depth, int *out) {
	if (toks[pos].type == PUR_T_LPAREN) {
		return parse_group(depth, out);
	}
	if (toks[pos].type == PUR_T_WORD) {
		return parse_leaf(out);
	}
	return PUR_PARSE_ERR_SYNTAX;
}

static int parse_group(int depth, int *out) {
	if (toks[pos].type != PUR_T_LPAREN) {
		return PUR_PARSE_ERR_SYNTAX;
	}
	pos++;

	if (toks[pos].type == PUR_T_RPAREN) {
		return PUR_PARSE_ERR_EMPTY_GROUP;
	}

	int members[PUR_MAX_GROUP_MEMBERS];
	int count = 0;
	int rc;

	while (1) {
		if (count >= PUR_MAX_GROUP_MEMBERS) {
			return PUR_PARSE_ERR_NODES;
		}

		rc = parse_stmt(depth, &members[count]);
		if (rc != PUR_PARSE_OK) {
			return rc;
		}
		count++;

		if (toks[pos].type != PUR_T_SEMI) {
			break;
		}

		pos++;
		if (toks[pos].type == PUR_T_RPAREN) {
			break;
		}
	}

	if (toks[pos].type != PUR_T_RPAREN) {
		return PUR_PARSE_ERR_SYNTAX;
	}
	pos++;

	int g = pur_node_new(PUR_NODE_GROUP);
	if (g < 0) {
		return PUR_PARSE_ERR_NODES;
	}

	if (pur_group_set(g, members, count) != 0) {
		return PUR_PARSE_ERR_NODES;
	}

	// a group with one member is transparent: (a) is just a
	*out = count == 1 ? members[0] : g;
	return PUR_PARSE_OK;
}

static int parse_stmt(int depth, int *out) {
	int node;
	int rc = parse_operand(depth, &node);
	if (rc != PUR_PARSE_OK) {
		return rc;
	}

	int fold = 0;

	while (toks[pos].type == PUR_T_GT) {
		fold++;

		int d = depth + fold;
		if (d > PUR_MAX_DEPTH) {
			return PUR_PARSE_ERR_COMPLEX;
		}

		pos++;

		if (toks[pos].type != PUR_T_WORD && toks[pos].type != PUR_T_LPAREN) {
			return PUR_PARSE_ERR_SYNTAX;
		}

		int rhs;
		rc = parse_operand(d, &rhs);
		if (rc != PUR_PARSE_OK) {
			return rc;
		}

		int e = pur_node_new(PUR_NODE_EXPR);
		if (e < 0) {
			return PUR_PARSE_ERR_NODES;
		}

		pur_nodes[e].first = node;
		pur_nodes[e].rhs = rhs;
		node = e;
	}

	*out = node;
	return PUR_PARSE_OK;
}

int pur_parse(char *text) {
	line = text;

	int rc = tokenize();
	if (rc != PUR_PARSE_OK) {
		return rc;
	}

	terminate_words();

	pos = 0;
	if (toks[0].type == PUR_T_END) {
		return PUR_PARSE_EMPTY;
	}

	int root;
	rc = parse_stmt(0, &root);
	if (rc != PUR_PARSE_OK) {
		return rc;
	}

	if (toks[pos].type != PUR_T_END) {
		return PUR_PARSE_ERR_SYNTAX;
	}

	pur_root = root;
	return PUR_PARSE_OK;
}

int pur_leaf_argv(int leaf, char **argv) {
	if (leaf < 0 || leaf >= pur_nleaves) {
		return -1;
	}

	pur_leaf_t *l = &pur_leaves[leaf];

	for (int i = 0; i < l->argc; i++) {
		argv[i] = &line[pur_argv_off[l->argv_first + i]];
	}

	return l->argc;
}

const char *pur_parse_strerror(int status) {
	switch (status) {
		case PUR_PARSE_OK:
			return "ok";
		case PUR_PARSE_EMPTY:
			return "empty";
		case PUR_PARSE_ERR_SYNTAX:
			return "syntax error";
		case PUR_PARSE_ERR_EMPTY_GROUP:
			return "an empty group has no leaves to wire";
		case PUR_PARSE_ERR_QUOTE:
			return "unterminated quote";
		case PUR_PARSE_ERR_TOKENS:
			return "too many tokens";
		case PUR_PARSE_ERR_NODES:
			return "statement too complex";
		case PUR_PARSE_ERR_LEAVES:
			return "too many commands in one statement";
		case PUR_PARSE_ERR_ARGV:
			return "too many arguments for one command";
		case PUR_PARSE_ERR_COMPLEX:
			return "statement too complex";
		default:
			return "unknown parse error";
	}
}
