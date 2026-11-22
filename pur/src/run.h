/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef PUR_RUN_H
#define PUR_RUN_H

// Walk a command that pur_parse_command() has already validated and run its
// expressions under the command operators. An expression followed by '%' is
// launched without waiting and its pids are added to the background job table.
// `dry_run` prints each expression's plan (with the operators between them)
// and runs nothing. The final status is the status of the last foreground
// expression that ran, and is stored in *status_out when it is not NULL.
// Returns 0, or -1 if an expression could not be loaded or planned.
int pur_run_command(bool dry_run, int *status_out);

#endif
