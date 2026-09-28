/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ast.h"

#include <stdlib.h>

pur_node_t* pur_nodes;
pur_leaf_t pur_leaves[PUR_MAX_LEAVES];
int* pur_argv_off;
int* pur_member_pool;

// how much each pool is holding right now
static size_t nodes_cap;
static size_t argv_cap;
static size_t members_cap;

int pur_nnodes;
int pur_nleaves;
int pur_nargv;
int pur_nmembers;
int pur_root;

// grow a pool so it can take `need` of its elements in all, doubling from
// whatever it holds now, or from PUR_AST_INIT if it holds nothing
static int pool_reserve(void** pool, size_t* cap, size_t need, size_t esz) {
	if (need <= *cap) {
		return 0;
	}

	size_t want = *cap ? *cap : PUR_AST_INIT;

	while (want < need) {
		// keep want from wrapping on its way round
		if (want > (size_t)-1 / 2 / esz) {
			return -1;
		}
		want *= 2;
	}

	void* p = realloc(*pool, want * esz);
	if (!p) {
		return -1;
	}

	*pool = p;
	*cap = want;
	return 0;
}

// the pools are kept across statements, so the next one in the shell reuses
// the room the last one took rather than going back to the allocator for it
void pur_ast_reset(void) {
	pur_nnodes = 0;
	pur_nleaves = 0;
	pur_nargv = 0;
	pur_nmembers = 0;
	pur_root = 0;
}

int pur_node_new(pur_kind_t kind) {
	if (pool_reserve((void**)&pur_nodes, &nodes_cap, (size_t)pur_nnodes + 1, sizeof(pur_node_t)) != 0) {
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
	if (pool_reserve((void**)&pur_argv_off, &argv_cap, (size_t)pur_nargv + 1, sizeof(int)) != 0) {
		return -1;
	}

	int idx = pur_nargv++;
	pur_argv_off[idx] = off;
	return idx;
}

int pur_group_set(int group_node, const int* members, int count) {
	if (count < 0) {
		return -1;
	}

	if (count > 0 &&
	    pool_reserve((void**)&pur_member_pool, &members_cap, (size_t)pur_nmembers + (size_t)count, sizeof(int)) != 0) {
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
