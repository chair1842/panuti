/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ast.h"

pur_node_t pur_nodes[PUR_MAX_NODES];
pur_leaf_t pur_leaves[PUR_MAX_LEAVES];
int pur_argv_off[PUR_MAX_LEAVES * PUR_MAX_ARGV];
int pur_member_pool[PUR_MAX_NODES];

int pur_nnodes;
int pur_nleaves;
int pur_nargv;
int pur_nmembers;
int pur_root;

void pur_ast_reset(void) {
	pur_nnodes = 0;
	pur_nleaves = 0;
	pur_nargv = 0;
	pur_nmembers = 0;
	pur_root = 0;
}

int pur_node_new(pur_kind_t kind) {
	if (pur_nnodes >= PUR_MAX_NODES) {
		return -1;
	}

	int n = pur_nnodes++;
	pur_nodes[n].kind = kind;
	pur_nodes[n].first = 0;
	pur_nodes[n].count = 0;
	pur_nodes[n].rhs = 0;
	return n;
}

int pur_leaf_new(int argv_first, int argc) {
	if (pur_nleaves >= PUR_MAX_LEAVES) {
		return -1;
	}

	int l = pur_nleaves++;
	pur_leaves[l].argv_first = argv_first;
	pur_leaves[l].argc = argc;
	return l;
}

int pur_argv_push(int off) {
	if (pur_nargv >= PUR_MAX_LEAVES * PUR_MAX_ARGV) {
		return -1;
	}

	int idx = pur_nargv++;
	pur_argv_off[idx] = off;
	return idx;
}

int pur_group_set(int group_node, const int *members, int count) {
	if (count < 0 || pur_nmembers + count > PUR_MAX_NODES) {
		return -1;
	}

	for (int i = 0; i < count; i++) {
		pur_member_pool[pur_nmembers + i] = members[i];
	}

	pur_nodes[group_node].first = pur_nmembers;
	pur_nodes[group_node].count = count;
	pur_nmembers += count;
	return 0;
}

bool pur_node_is_leaf(int node) {
	return pur_nodes[node].kind == PUR_NODE_LEAF;
}

int pur_member(int group_node, int i) {
	return pur_member_pool[pur_nodes[group_node].first + i];
}
