/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef PUR_AST_H
#define PUR_AST_H

#include <stdbool.h>

#define PUR_MAX_TOKENS 256
#define PUR_MAX_NODES 256
#define PUR_MAX_LEAVES 16
#define PUR_MAX_PIPES 14
#define PUR_MAX_STREAMS 16
#define PUR_MAX_DEPTH 16
#define PUR_MAX_ARGV 32
#define PUR_LINE_MAX 4096

typedef enum {
	PUR_NODE_LEAF = 0,
	PUR_NODE_GROUP = 1,
	PUR_NODE_EXPR = 2,
} pur_kind_t;

typedef struct {
	pur_kind_t kind;
	int first;
	int count;
	int rhs;
} pur_node_t;

typedef struct {
	int argv_first;
	int argc;
} pur_leaf_t;

extern pur_node_t pur_nodes[PUR_MAX_NODES];
extern pur_leaf_t pur_leaves[PUR_MAX_LEAVES];
extern int pur_argv_off[PUR_MAX_LEAVES * PUR_MAX_ARGV];
extern int pur_member_pool[PUR_MAX_NODES];

extern int pur_nnodes;
extern int pur_nleaves;
extern int pur_nargv;
extern int pur_nmembers;
extern int pur_root;

void pur_ast_reset(void);
int pur_node_new(pur_kind_t kind);
int pur_leaf_new(int argv_first, int argc);
int pur_argv_push(int off);
int pur_group_set(int group_node, const int *members, int count);
bool pur_node_is_leaf(int node);
int pur_member(int group_node, int i);

#endif
