/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "run.h"
#include "exec.h"
#include "parse.h"

#include <stdio.h>

// Walk a full command, one expression at a time. The operator before an
// expression decides whether it runs; a skipped expression leaves the status
// alone. pur_exec() runs a whole expression - a pipeline or a group - and the
// only thing that changes between expressions is the AST, so pur_parse_load()
// resets the pool and re-parses the next one.
int pur_run_command(bool dry_run, int *status_out) {
	int count = pur_parse_nexprs();
	int status = 0;

	for (int i = 0; i < count; i++) {
		if (i > 0 && !pur_op_should_run(pur_parse_op(i - 1), status)) {
			continue;
		}

		int rc = pur_parse_load(i);
		if (rc != PUR_PARSE_OK) {
			printf("pur: %s\n", pur_parse_strerror(rc));
			return -1;
		}

		if (dry_run) {
			int xrc = pur_exec_plan();
			if (xrc != PUR_EXEC_OK) {
				printf("pur: %s\n", pur_exec_strerror(xrc));
				return -1;
			}
		} else {
			int st = 0;
			int xrc = pur_exec(&st);

			// a command that never started is a failure too: say so, and
			// keep the chain going so the next operator can see it
			if (xrc != PUR_EXEC_OK) {
				printf("pur: %s\n", pur_exec_strerror(xrc));
				status = xrc == PUR_EXEC_ERR_NOTFOUND ? 127 : 1;
			} else {
				status = st;
			}
		}

		// -n shows the join between expressions, so nothing after the last
		if (dry_run && i + 1 < count) {
			printf("%s\n", pur_parse_op_str(pur_parse_op(i)));
		}
	}

	if (status_out) {
		*status_out = status;
	}

	return 0;
}
