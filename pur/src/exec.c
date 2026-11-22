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
#include <stdlib.h>
#include <string.h>

#define PUR_PATH "/cd/usr/bin/"
#define PUR_DEFAULT_IN "/dvc/kbd/line"
#define PUR_BACKGROUND_IN "/dvc/null"
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

static pur_edge_t* edges;
static int nedges;
static size_t edges_cap;

static int* in_count;
static int* out_count;
static int* in_fd;
static int* out_fd;
static pid_t* pids;
static bool* launched;
static int* extra_fds;
static int* scratch;

static int nextra_fds;
static int state_leaves;

#define IN_FD(leaf, stream) in_fd[(size_t)(leaf) * PUR_MAX_STREAMS + (stream)]
#define OUT_FD(leaf, stream) out_fd[(size_t)(leaf) * PUR_MAX_STREAMS + (stream)]
#define IN_FDS(leaf) (&in_fd[(size_t)(leaf) * PUR_MAX_STREAMS])
#define OUT_FDS(leaf) (&out_fd[(size_t)(leaf) * PUR_MAX_STREAMS])

static void state_free(void) {
	free(edges);
	free(in_count);
	free(out_count);
	free(in_fd);
	free(out_fd);
	free(pids);
	free(launched);
	free(extra_fds);
	free(scratch);

	edges = nullptr;
	in_count = nullptr;
	out_count = nullptr;
	in_fd = nullptr;
	out_fd = nullptr;
	pids = nullptr;
	launched = nullptr;
	extra_fds = nullptr;
	scratch = nullptr;

	edges_cap = 0;
	nedges = 0;
	nextra_fds = 0;
	state_leaves = 0;
}

static bool state_alloc(int leaves) {
	state_free();

	if (leaves <= 0) {
		return true;
	}

	size_t n = (size_t)leaves;
	if (n > (size_t)-1 / PUR_MAX_STREAMS / sizeof(int) ||
	    n > (size_t)-1 / 2 / sizeof(int)) {
		return false;
	}

	in_count = calloc(n, sizeof(int));
	out_count = calloc(n, sizeof(int));
	in_fd = calloc(n * PUR_MAX_STREAMS, sizeof(int));
	out_fd = calloc(n * PUR_MAX_STREAMS, sizeof(int));
	pids = malloc(n * sizeof(pid_t));
	launched = calloc(n, sizeof(bool));
	extra_fds = malloc(n * 2 * sizeof(int));
	scratch = malloc(n * sizeof(int));

	if (!in_count || !out_count || !in_fd || !out_fd || !pids || !launched ||
	    !extra_fds || !scratch) {
		state_free();
		return false;
	}

	state_leaves = leaves;
	for (int i = 0; i < leaves; i++) {
		pids[i] = -1;
		for (int j = 0; j < 2; j++) {
			extra_fds[i * 2 + j] = -1;
		}
		for (int s = 0; s < PUR_MAX_STREAMS; s++) {
			IN_FD(i, s) = -1;
			OUT_FD(i, s) = -1;
		}
	}

	return true;
}

static bool edges_reserve(size_t need) {
	if (need <= edges_cap) {
		return true;
	}

	size_t want = edges_cap ? edges_cap : PUR_MAX_PIPES;
	while (want < need) {
		if (want > (size_t)-1 / 2 / sizeof(pur_edge_t)) {
			return false;
		}
		want *= 2;
	}

	pur_edge_t* p = realloc(edges, want * sizeof(pur_edge_t));
	if (!p) {
		return false;
	}

	edges = p;
	edges_cap = want;
	return true;
}

static void reset_state(void) {
	nedges = 0;
	nextra_fds = 0;

	for (int i = 0; i < state_leaves; i++) {
		in_count[i] = 0;
		out_count[i] = 0;
		launched[i] = false;
		pids[i] = -1;
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

	if (!edges_reserve((size_t)nedges + 1)) {
		return PUR_EXEC_ERR_MEMORY;
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
		if (*n >= state_leaves) {
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

	int nouts = 0;

	rc = outputs_collect(p->first, scratch, &nouts);
	if (rc != PUR_EXEC_OK) {
		return rc;
	}

	// inside a compound the right hand side is downstream of the left, so
	// the edge reaches through any group all the way down
	return deliver_internal(scratch, nouts, p->rhs);
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
	printf("operators:\n");
	printf("  cmd && cmd       - run the next only if the status so far is 0\n");
	printf("  cmd || cmd       - run the next only if the status is not 0\n");
	printf("  cmd & cmd        - then run the next, whatever the status\n");
	printf("  cmd %% cmd        - run cmd in the background, then the next\n");
	printf("                     background jobs read /dvc/null and are reaped\n");
	printf("                     with waitp_many before the next prompt\n\n");
	printf("  they share one precedence and are left associative. '|' is not\n");
	printf("  used at all: pur pipes with '>'. a trailing '%%' is fine; a\n");
	printf("  trailing '&', '&&' or '||' is an error.\n\n");
	printf("usage:\n");
	printf("  pur [-h] [-v] [-n <statement>] [-c <statement>] [<script>]\n\n");
	printf("  -n wires <statement> and prints each command's stream counts\n");
	printf("     without running anything\n\n");
	printf("  -c runs <statement> and exits with the pipeline's status\n\n");
	printf("  <script> runs each line of the file, then exits. blank lines and\n");
	printf("     lines starting with '#' are ignored\n\n");
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

static int wire_defaults(bool background) {
	const char* default_in = background ? PUR_BACKGROUND_IN : PUR_DEFAULT_IN;

	for (int i = 0; i < pur_nleaves; i++) {
		if (in_count[i] == 0) {
			if (open_default(default_in, &IN_FD(i, 0)) != 0) {
				return -1;
			}
			in_count[i] = 1;
		}

		if (out_count[i] == 0) {
			if (open_default(PUR_DEFAULT_OUT, &OUT_FD(i, 0)) != 0) {
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

		OUT_FD(edges[i].src, edges[i].src_stream) = wfd;
		IN_FD(edges[i].dst, edges[i].dst_stream) = rfd;
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
	if (!state_alloc(pur_nleaves)) {
		return PUR_EXEC_ERR_MEMORY;
	}

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

	int nouts = 0;

	rc = outputs_collect(pur_nodes[pur_root].first, scratch, &nouts);
	if (rc != PUR_EXEC_OK) {
		return rc;
	}

	return deliver_external(scratch, nouts, pur_nodes[pur_root].rhs);
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

static int exec_start(bool background, int* status_out, pid_t** bg_pids_out,
		      int* count_out) {
	if (status_out) {
		*status_out = 0;
	}
	if (count_out) {
		*count_out = 0;
	}
	if (bg_pids_out) {
		*bg_pids_out = nullptr;
	}

	if (background && (!bg_pids_out || !count_out)) {
		return PUR_EXEC_ERR_JOBS;
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

	if (wire_defaults(background) != 0) {
		close_all();
		return PUR_EXEC_ERR_PROCREATE;
	}

	if (create_pipes() != 0) {
		close_all();
		return PUR_EXEC_ERR_PIPE_CREATE;
	}

	if (background && pur_nleaves > 0) {
		*bg_pids_out = malloc((size_t)pur_nleaves * sizeof(pid_t));
		if (!*bg_pids_out) {
			close_all();
			return PUR_EXEC_ERR_MEMORY;
		}
	}

	for (int i = 0; i < pur_nleaves; i++) {
		int argc = pur_leaf_argv(i, argv);
		resolve_path(argv[0], path, sizeof(path));

		pid_t pid = procreate(path, argv, argc, IN_FDS(i), in_count[i],
				      OUT_FDS(i), out_count[i]);

		if ((int32_t)pid < 0) {
			printf("pur: %s: could not start (error %d)\n", argv[0], (int)pid);
			// children already launched are still returned so the caller
			// can put them in the job table and reap them later
			close_all();
			return PUR_EXEC_ERR_PROCREATE;
		}

		pids[i] = pid;
		launched[i] = true;

		if (background) {
			(*bg_pids_out)[(*count_out)++] = pid;
		}
	}

	close_all();

	// background jobs are handed to the caller; this shell reaps them at the
	// next prompt instead of blocking here
	if (background) {
		return PUR_EXEC_OK;
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

int pur_exec(int* status_out) {
	return exec_start(false, status_out, NULL, NULL);
}

int pur_exec_background(pid_t** pids_out, int* count_out) {
	return exec_start(true, NULL, pids_out, count_out);
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
		case PUR_EXEC_ERR_JOBS:
			return "too many background processes";
		case PUR_EXEC_ERR_MEMORY:
			return "out of memory";
		default:
			return "unknown error";
	}
}
