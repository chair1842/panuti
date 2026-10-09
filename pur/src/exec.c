/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "exec.h"
#include "ast.h"
#include "parse.h"

#include <panuti/errno.h>
#include <panuti/handle.h>
#include <panuti/stat.h>
#include <panuti/inode_type.h>
#include <panuti/process.h>
#include <panuti/syscall/syscallsf.h>

#include <stdio.h>
#include <string.h>

#define PUR_PATH "/cd/usr/bin/"
#define PUR_DEFAULT_IN "/dvc/kbd/line"
#define PUR_DEFAULT_OUT "/dvc/console"
#define PUR_PATH_MAX 128

typedef struct {
	int src;
	int dst;
	int src_stream;
	int dst_stream;
	int rfd;
	int wfd;
} pur_edge_t;

static pur_edge_t edges[PUR_MAX_PIPES];
static int nedges;

static int in_count[PUR_MAX_LEAVES];
static int out_count[PUR_MAX_LEAVES];
static int in_fd[PUR_MAX_LEAVES][PUR_MAX_STREAMS];
static int out_fd[PUR_MAX_LEAVES][PUR_MAX_STREAMS];

static pid_t pids[PUR_MAX_LEAVES];
static bool launched[PUR_MAX_LEAVES];

static int extra_fds[PUR_MAX_LEAVES * 2];
static int nextra_fds;

static void reset_state(void) {
	nedges = 0;
	nextra_fds = 0;

	for (int i = 0; i < PUR_MAX_LEAVES; i++) {
		in_count[i] = 0;
		out_count[i] = 0;
		launched[i] = false;
		pids[i] = -1;
	}

	for (int i = 0; i < PUR_MAX_LEAVES * 2; i++) {
		extra_fds[i] = -1;
	}
}

static int add_edge(int src, int dst) {
	if (src == dst) {
		return PUR_EXEC_OK;
	}

	for (int i = 0; i < nedges; i++) {
		if (edges[i].src == src && edges[i].dst == dst) {
			return PUR_EXEC_OK;
		}
	}

	if (nedges >= PUR_MAX_PIPES) {
		return PUR_EXEC_ERR_PIPES;
	}

	if (out_count[src] >= PUR_MAX_STREAMS || in_count[dst] >= PUR_MAX_STREAMS) {
		return PUR_EXEC_ERR_STREAMS;
	}

	pur_edge_t *e = &edges[nedges];
	e->src = src;
	e->dst = dst;
	e->src_stream = out_count[src]++;
	e->dst_stream = in_count[dst]++;
	e->rfd = -1;
	e->wfd = -1;
	nedges++;

	return PUR_EXEC_OK;
}

static int outputs_collect(int node, int *out, int *n) {
	pur_node_t *p = &pur_nodes[node];

	if (p->kind == PUR_NODE_LEAF) {
		if (*n >= PUR_MAX_LEAVES) {
			return PUR_EXEC_ERR_STREAMS;
		}
		out[(*n)++] = p->first;
		return PUR_EXEC_OK;
	}

	if (p->kind == PUR_NODE_GROUP) {
		for (int i = 0; i < p->count; i++) {
			int rc = outputs_collect(pur_member(node, i), out, n);
			if (rc != PUR_EXEC_OK) {
				return rc;
			}
		}
		return PUR_EXEC_OK;
	}

	return outputs_collect(p->rhs, out, n);
}

static int deliver_external(const int* srcs, int nsrc, int node);

// Wire one statement's own output into its right hand side. An internal edge
// flows through groups all the way down, because inside a compound every
// member is downstream of the same source.
static int deliver_internal(const int* srcs, int nsrc, int node) {
	pur_node_t* p = &pur_nodes[node];

	if (p->kind == PUR_NODE_LEAF) {
		for (int i = 0; i < nsrc; i++) {
			int rc = add_edge(srcs[i], p->first);
			if (rc != PUR_EXEC_OK) {
				return rc;
			}
		}
		return PUR_EXEC_OK;
	}

	if (p->kind == PUR_NODE_GROUP) {
		for (int i = 0; i < p->count; i++) {
			int rc = deliver_internal(srcs, nsrc, pur_member(node, i));
			if (rc != PUR_EXEC_OK) {
				return rc;
			}
		}
		return PUR_EXEC_OK;
	}

	return deliver_internal(srcs, nsrc, p->rhs);
}

// Wire a stream that arrives from outside the compound into the commands
// that can actually read it: the bare leaves directly reachable from the
// sink position. A group one level down is a boundary, because what sits
// behind it is that group's own business.
static int wire_bare_sides(const int* srcs, int nsrc, int node) {
	pur_node_t* p = &pur_nodes[node];

	int rc = PUR_EXEC_OK;

	if (pur_node_is_leaf(p->first)) {
		rc = deliver_external(srcs, nsrc, p->first);
		if (rc != PUR_EXEC_OK) {
			return rc;
		}
	}

	if (pur_node_is_leaf(p->rhs)) {
		rc = deliver_external(srcs, nsrc, p->rhs);
		if (rc != PUR_EXEC_OK) {
			return rc;
		}
	}

	return PUR_EXEC_OK;
}

static int deliver_external(const int* srcs, int nsrc, int node) {
	pur_node_t* p = &pur_nodes[node];

	if (p->kind == PUR_NODE_LEAF) {
		for (int i = 0; i < nsrc; i++) {
			int rc = add_edge(srcs[i], p->first);
			if (rc != PUR_EXEC_OK) {
				return rc;
			}
		}
		return PUR_EXEC_OK;
	}

	if (p->kind == PUR_NODE_GROUP) {
		for (int i = 0; i < p->count; i++) {
			int member = pur_member(node, i);
			int rc;

			if (pur_node_is_leaf(member)) {
				rc = deliver_external(srcs, nsrc, member);
			} else if (pur_nodes[member].kind == PUR_NODE_EXPR) {
				rc = wire_bare_sides(srcs, nsrc, member);
			} else {
				rc = PUR_EXEC_OK;
			}

			if (rc != PUR_EXEC_OK) {
				return rc;
			}
		}
		return PUR_EXEC_OK;
	}

	return wire_bare_sides(srcs, nsrc, node);
}

// wire_self is false for the root: the root's own '>' is a boundary between
// what came before and the compound on the right, so it is wired by
// deliver_external instead.
static int wire_internals(int node, bool wire_self) {
	pur_node_t *p = &pur_nodes[node];

	if (p->kind == PUR_NODE_GROUP) {
		for (int i = 0; i < p->count; i++) {
			int rc = wire_internals(pur_member(node, i), true);
			if (rc != PUR_EXEC_OK) {
				return rc;
			}
		}
		return PUR_EXEC_OK;
	}

	if (p->kind != PUR_NODE_EXPR) {
		return PUR_EXEC_OK;
	}

	int rc = wire_internals(p->first, true);
	if (rc != PUR_EXEC_OK) {
		return rc;
	}

	rc = wire_internals(p->rhs, true);
	if (rc != PUR_EXEC_OK) {
		return rc;
	}

	if (!wire_self) {
		return PUR_EXEC_OK;
	}

	int outs[PUR_MAX_LEAVES];
	int nouts = 0;

	rc = outputs_collect(p->first, outs, &nouts);
	if (rc != PUR_EXEC_OK) {
		return rc;
	}

	// inside a compound the right hand side is downstream of the left, so
	// the edge reaches through any group all the way down
	return deliver_internal(outs, nouts, p->rhs);
}

static bool is_builtin(const char* name) {
	return strcmp(name, "help") == 0 || strcmp(name, "exit") == 0 ||
	       strcmp(name, "cd") == 0;
}

int pur_builtin_help(void) {
	printf("pur 0.2 - the panuti shell\n\n");
	printf("shell built-ins:\n");
	printf("  help    - show this message\n");
	printf("  exit    - exit pur, it will come back anyways\n");
	printf("  cd      - change the current working directory\n\n");
	printf("pipelines:\n");
	printf("  cmd > cmd        - wire cmd's output into cmd's input\n");
	printf("  cmd              - run cmd with %s\n", PUR_DEFAULT_IN);
	printf("  (cmd ; cmd)      - run both, each with %s and %s\n", PUR_DEFAULT_IN,
	       PUR_DEFAULT_OUT);
	printf("  (cmd > cmd ; cmd) - wire the pipeline into cmd\n\n");
	printf("  '>' is left associative, and a command's output reaches every\n");
	printf("  command downstream of it. quotes keep spaces and punctuation.\n\n");
	printf("usage:\n");
	printf("  pur [-h] [-v] [-n <statement>] [-c <statement>]\n\n");
	printf("  -n wires <statement> and prints each command's stream counts\n");
	printf("     without running anything\n\n");
	printf("  -c runs <statement> and exits with the pipeline's status\n\n");
	printf("  run from inside pur, quote the statement, or the '>' belongs to\n");
	printf("  the outer pur: pur -c \"in /cd/file > cat > more\"\n\n");
	printf("unwired commands get %s in and %s out\n", PUR_DEFAULT_IN,
	       PUR_DEFAULT_OUT);
	return 0;
}

static int builtin_cd(int argc, char** argv) {
	if (argc < 2) {
		printf("pur: no path provided for cd\n");
		return -1;
	}

	if (argc > 2) {
		printf("pur: why do you have more than one arg?\n");
	}

	switch (panutisysf_chdir(argv[1])) {
		case PANUTIERRNO_PLAINSUCCESS:
			return 0;
		case PANUTIERRNO_NOTFOUND:
			printf("pur: path not found\n");
			return -1;
		case PANUTIERRNO_UNSUPPORTEDOP:
			printf("pur: path not a directory\n");
			return -1;
		case PANUTIERRNO_INVALIDADDR:
			printf("pur: what the chicken is this.\n");
			return -1;
		default:
			printf("pur: could not change the directory\n");
			return -1;
	}
}

static int run_builtin(int leaf) {
	char* argv[PUR_MAX_ARGV];
	int argc = pur_leaf_argv(leaf, argv);

	if (strcmp(argv[0], "exit") == 0) {
		panutisysf_exit(0);
	}

	if (strcmp(argv[0], "cd") == 0) {
		return builtin_cd(argc, argv);
	}

	return pur_builtin_help();
}

static bool is_explicit_path(const char* s) {
	if (s[0] == '.') {
		return true;
	}

	for (const char* p = s; *p; p++) {
		if (*p == '/') {
			return true;
		}
	}

	return false;
}

static int resolve_path(const char* name, char* out, size_t outsz) {
	if (is_explicit_path(name)) {
		if (strlen(name) + 1 > outsz) {
			return -1;
		}
		strcpy(out, name);
		return 0;
	}

	size_t dirlen = strlen(PUR_PATH);
	size_t namelen = strlen(name);

	if (dirlen + namelen + 1 > outsz) {
		return -1;
	}

	for (size_t i = 0; i < dirlen; i++) {
		out[i] = PUR_PATH[i];
	}
	for (size_t i = 0; i < namelen; i++) {
		out[dirlen + i] = name[i];
	}
	out[dirlen + namelen] = '\0';

	return 0;
}

static int check_leaf(int leaf, char* path, size_t pathsz) {
	char* argv[PUR_MAX_ARGV];
	int argc = pur_leaf_argv(leaf, argv);

	if (argc < 1) {
		return PUR_EXEC_ERR_NOTFOUND;
	}

	if (resolve_path(argv[0], path, pathsz) != 0) {
		printf("pur: %s: name too long\n", argv[0]);
		return PUR_EXEC_ERR_NOTFOUND;
	}

	// nexist is the cheap answer, and a miss is the common case while walking
	// PATH, so ask it first and only spend a stat telling a directory apart
	if (!nexist(path)) {
		printf("pur: %s: command not found (%s)\n", argv[0], path);
		return PUR_EXEC_ERR_NOTFOUND;
	}

	dirent_entry_t entry;
	if (stat(path, &entry) == 0 && entry.type == INODE_DIR) {
		printf("pur: %s: command provided is a directory (%s)\n", argv[0], path);
		return PUR_EXEC_ERR_ISDIR;
	}

	return PUR_EXEC_OK;
}

static int open_default(const char* path, int* dest) {
	int fd = handle_open(path);

	if (fd < 0) {
		printf("pur: %s: could not open default stream\n", path);
		return -1;
	}

	extra_fds[nextra_fds++] = fd;
	*dest = fd;
	return 0;
}

static int wire_defaults(void) {
	for (int i = 0; i < pur_nleaves; i++) {
		if (in_count[i] == 0) {
			if (open_default(PUR_DEFAULT_IN, &in_fd[i][0]) != 0) {
				return -1;
			}
			in_count[i] = 1;
		}

		if (out_count[i] == 0) {
			if (open_default(PUR_DEFAULT_OUT, &out_fd[i][0]) != 0) {
				return -1;
			}
			out_count[i] = 1;
		}
	}

	return 0;
}

static int create_pipes(void) {
	for (int i = 0; i < nedges; i++) {
		int rfd = -1;
		int wfd = -1;

		if (panutisysf_pipe_create(&rfd, &wfd) != 0) {
			return -1;
		}

		edges[i].rfd = rfd;
		edges[i].wfd = wfd;

		out_fd[edges[i].src][edges[i].src_stream] = wfd;
		in_fd[edges[i].dst][edges[i].dst_stream] = rfd;
	}

	return 0;
}

static void close_all(void) {
	for (int i = 0; i < nedges; i++) {
		if (edges[i].rfd >= 0) {
			handle_close(edges[i].rfd);
			edges[i].rfd = -1;
		}
		if (edges[i].wfd >= 0) {
			handle_close(edges[i].wfd);
			edges[i].wfd = -1;
		}
	}

	for (int i = 0; i < nextra_fds; i++) {
		if (extra_fds[i] >= 0) {
			handle_close(extra_fds[i]);
			extra_fds[i] = -1;
		}
	}
}

static int wire_graph(void) {
	reset_state();

	if (pur_nleaves == 0) {
		return PUR_EXEC_OK;
	}

	if (pur_nodes[pur_root].kind != PUR_NODE_EXPR) {
		return PUR_EXEC_OK;
	}

	int rc = wire_internals(pur_root, false);
	if (rc != PUR_EXEC_OK) {
		return rc;
	}

	int outs[PUR_MAX_LEAVES];
	int nouts = 0;

	rc = outputs_collect(pur_nodes[pur_root].first, outs, &nouts);
	if (rc != PUR_EXEC_OK) {
		return rc;
	}

	return deliver_external(outs, nouts, pur_nodes[pur_root].rhs);
}

// -n: wire the statement, print the per command stream counts, run nothing
int pur_exec_plan(void) {
	int rc = wire_graph();
	if (rc != PUR_EXEC_OK) {
		return rc;
	}

	for (int i = 0; i < pur_nleaves; i++) {
		char* argv[PUR_MAX_ARGV];
		int argc = pur_leaf_argv(i, argv);

		printf("%s in=%d out=%d\n", argc > 0 ? argv[0] : "?", in_count[i],
		       out_count[i]);
	}

	return PUR_EXEC_OK;
}

int pur_exec(int* status_out) {
	if (status_out) {
		*status_out = 0;
	}

	int rc = wire_graph();
	if (rc != PUR_EXEC_OK) {
		return rc;
	}

	if (pur_nleaves == 0) {
		return PUR_EXEC_OK;
	}

	// an unwired single command is only in process if it is a built-in
	if (nedges == 0 && pur_nleaves == 1) {
		char* av[PUR_MAX_ARGV];

		if (pur_leaf_argv(0, av) > 0 && is_builtin(av[0])) {
			int brc = run_builtin(0);
			if (brc != 0 && status_out) {
				*status_out = 1;
			}
			return PUR_EXEC_OK;
		}
	}

	char* argv[PUR_MAX_ARGV];

	for (int i = 0; i < pur_nleaves; i++) {
		if (pur_leaf_argv(i, argv) < 1) {
			return PUR_EXEC_ERR_NOTFOUND;
		}

		if (is_builtin(argv[0])) {
			printf("pur: %s: a built-in cannot run inside a pipeline\n", argv[0]);
			return PUR_EXEC_ERR_BUILTIN;
		}
	}

	char path[PUR_PATH_MAX];

	for (int i = 0; i < pur_nleaves; i++) {
		rc = check_leaf(i, path, sizeof(path));
		if (rc != PUR_EXEC_OK) {
			return rc;
		}
	}

	if (wire_defaults() != 0) {
		close_all();
		return PUR_EXEC_ERR_PROCREATE;
	}

	if (create_pipes() != 0) {
		close_all();
		return PUR_EXEC_ERR_PIPE_CREATE;
	}

	for (int i = 0; i < pur_nleaves; i++) {
		int argc = pur_leaf_argv(i, argv);
		resolve_path(argv[0], path, sizeof(path));

		pid_t pid = procreate(path, argv, argc, in_fd[i], in_count[i], out_fd[i],
				      out_count[i]);

		if ((int32_t)pid < 0) {
			printf("pur: %s: could not start (error %d)\n", argv[0], (int)pid);
			close_all();
			return PUR_EXEC_ERR_PROCREATE;
		}

		pids[i] = pid;
		launched[i] = true;
	}

	close_all();

	if (status_out) {
		*status_out = 0;
	}

	int lowest = 0;

	for (int i = 0; i < pur_nleaves; i++) {
		if (!launched[i]) {
			continue;
		}

		int ec = 0;
		wait(pids[i], &ec);

		if (ec != 0 && (lowest == 0 || ec < lowest)) {
			lowest = ec;
		}
	}

	if (status_out) {
		*status_out = lowest;
	}

	return PUR_EXEC_OK;
}

const char* pur_exec_strerror(int status) {
	switch (status) {
		case PUR_EXEC_OK:
			return "ok";
		case PUR_EXEC_ERR_PIPES:
			return "too many pipes in one statement";
		case PUR_EXEC_ERR_STREAMS:
			return "too many streams for one command";
		case PUR_EXEC_ERR_NOTFOUND:
			return "command not found";
		case PUR_EXEC_ERR_ISDIR:
			return "is a directory";
		case PUR_EXEC_ERR_PIPE_CREATE:
			return "could not create a pipe";
		case PUR_EXEC_ERR_PROCREATE:
			return "could not start a command";
		case PUR_EXEC_ERR_BUILTIN:
			return "a built-in cannot run inside a pipeline";
		default:
			return "unknown error";
	}
}
