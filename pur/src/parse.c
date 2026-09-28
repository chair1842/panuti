/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "parse.h"
#include "ast.h"

#include <stddef.h>
#include <stdlib.h>

#define PUR_GROUP_INIT 8

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

// the tokens of a statement, kept on the heap and grown as they are found
static pur_token_t* toks;
static size_t toks_cap;
static int ntoks;
static int pos;
static char *line;

// make room for one more token
static int toks_reserve(size_t need) {
	if (need <= toks_cap) {
		return 0;
	}

	size_t cap = toks_cap ? toks_cap : PUR_AST_INIT;

	while (cap < need) {
		if (cap > (size_t)-1 / 2 / sizeof(pur_token_t)) {
			return -1;
		}
		cap *= 2;
	}

	pur_token_t* p = realloc(toks, cap * sizeof(pur_token_t));
	if (!p) {
		return -1;
	}

	toks = p;
	toks_cap = cap;
	return 0;
}

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

		if (toks_reserve((size_t)ntoks + 1) != 0) {
			return PUR_PARSE_ERR_MEMORY;
		}

		toks[ntoks].type = (pur_tok_t)type;
		toks[ntoks].off = off;
		toks[ntoks].len = len;
		ntoks++;
	}

	if (toks_reserve((size_t)ntoks + 1) != 0) {
		return PUR_PARSE_ERR_MEMORY;
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

// grow the members of a group so it can take `need` of them in all
static int members_reserve(int** members, size_t* cap, size_t need) {
	if (need <= *cap) {
		return 0;
	}

	size_t want = *cap ? *cap : PUR_GROUP_INIT;

	while (want < need) {
		if (want > (size_t)-1 / 2 / sizeof(int)) {
			return -1;
		}
		want *= 2;
	}

	int* p = realloc(*members, want * sizeof(int));
	if (!p) {
		return -1;
	}

	*members = p;
	*cap = want;
	return 0;
}

static int parse_group(int depth, int *out) {
	if (toks[pos].type != PUR_T_LPAREN) {
		return PUR_PARSE_ERR_SYNTAX;
	}
	pos++;

	if (toks[pos].type == PUR_T_RPAREN) {
		return PUR_PARSE_ERR_EMPTY_GROUP;
	}

	// the members are on the heap, because a group can sit inside a group
	// up to PUR_MAX_DEPTH deep, and a stack array at every one of those
	// levels is room the shell does not have to spare
	int* members = NULL;
	size_t members_cap = 0;
	size_t count = 0;
	int single = 0;
	int g = -1;
	int rc = PUR_PARSE_OK;

	while (1) {
		if (members_reserve(&members, &members_cap, count + 1) != 0) {
			rc = PUR_PARSE_ERR_MEMORY;
			break;
		}

		rc = parse_stmt(depth, &members[count]);
		if (rc != PUR_PARSE_OK) {
			break;
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

	if (rc == PUR_PARSE_OK && toks[pos].type != PUR_T_RPAREN) {
		rc = PUR_PARSE_ERR_SYNTAX;
	}

	if (rc == PUR_PARSE_OK) {
		pos++;

		// a group with one member is transparent: (a) is just a
		single = count == 1 ? members[0] : 0;

		g = pur_node_new(PUR_NODE_GROUP);
		if (g < 0) {
			rc = PUR_PARSE_ERR_NODES;
		} else if (pur_group_set(g, members, (int)count) != 0) {
			rc = PUR_PARSE_ERR_NODES;
		}
	}

	free(members);

	if (rc != PUR_PARSE_OK) {
		return rc;
	}

	// count decides which, not single: a single member can be node 0, which
	// is a perfectly good index and would read as false
	*out = count == 1 ? single : g;
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
		case PUR_PARSE_ERR_NODES:
			return "statement too complex";
		case PUR_PARSE_ERR_LEAVES:
			return "too many commands in one statement";
		case PUR_PARSE_ERR_ARGV:
			return "too many arguments for one command";
		case PUR_PARSE_ERR_COMPLEX:
			return "statement too complex";
		case PUR_PARSE_ERR_MEMORY:
			return "out of memory";
		default:
			return "unknown parse error";
	}
}
