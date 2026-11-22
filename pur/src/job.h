/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef PUR_JOB_H
#define PUR_JOB_H

#include <panuti/process.h>
#include <stdbool.h>

// Background pids are kept on the heap until waitp_many() reports that they
// exited. The table keeps its capacity between prompts.
bool pur_job_add(const pid_t* pids, int count);
void pur_job_reap(void);

#endif
