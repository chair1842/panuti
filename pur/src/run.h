/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef PUR_RUN_H
#define PUR_RUN_H

// Walk a command that pur_parse_command() has already validated and run its
// expressions under the command operators. `dry_run` prints each expression's
// plan (with the operators between them) and runs nothing. The final status is
// the status of the last expression that ran, and is stored in *status_out
// when it is not NULL. Returns 0, or -1 if an expression could not be loaded
// or planned.
int pur_run_command(bool dry_run, int *status_out);

#endif
