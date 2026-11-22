/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef PUR_EXEC_H
#define PUR_EXEC_H

#include <panuti/process.h>

typedef enum {
	PUR_EXEC_OK = 0,
	PUR_EXEC_ERR_PIPES,
	PUR_EXEC_ERR_STREAMS,
	PUR_EXEC_ERR_NOTFOUND,
	PUR_EXEC_ERR_ISDIR,
	PUR_EXEC_ERR_PIPE_CREATE,
	PUR_EXEC_ERR_PROCREATE,
	PUR_EXEC_ERR_BUILTIN,
	PUR_EXEC_ERR_JOBS,
	PUR_EXEC_ERR_MEMORY,
} pur_exec_status_t;

// Runs the parsed statement. Returns PUR_EXEC_OK, or one of the positive
// PUR_EXEC_ERR_* codes, so callers must test against PUR_EXEC_OK and never
// against < 0. The pipeline's exit status (lowest nonzero child code, 0 if all
// succeeded) is stored in *status_out.
int pur_exec(int* status_out);

// Launch the whole parsed expression without waiting for it. *pids_out is set
// to a freshly allocated array of child pids and *count_out to its length; the
// caller owns that array and should reap the pids later. A built-in expression
// runs synchronously and produces no pids.
int pur_exec_background(pid_t** pids_out, int* count_out);

int pur_exec_plan(void);
int pur_builtin_help(void);
const char* pur_exec_strerror(int status);

#endif
