/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "job.h"

#include <panuti/errno.h>
#include <panuti/process.h>

static pid_t job_pids[PUR_JOB_MAX];
static int job_count;

bool pur_job_add(const pid_t* pids, int count) {
	if (!pids || count <= 0) {
		return true;
	}

	if (job_count + count > PUR_JOB_MAX) {
		return false;
	}

	for (int i = 0; i < count; i++) {
		job_pids[job_count++] = pids[i];
	}

	return true;
}

static void job_remove(pid_t pid) {
	for (int i = 0; i < job_count; i++) {
		if (job_pids[i] != pid) {
			continue;
		}

		for (int j = i + 1; j < job_count; j++) {
			job_pids[j - 1] = job_pids[j];
		}

		job_count--;
		return;
	}
}

void pur_job_reap(void) {
	while (job_count > 0) {
		int exit_code;
		pid_t exited_pid;

		int32_t rc = waitp_many(job_pids, (uint32_t)job_count, &exit_code,
					&exited_pid);

		if (rc == (int32_t)PANUTIERRNO_BUSY) {
			break;
		}

		// every pid in the table is stale: nothing can be reaped
		if (rc == (int32_t)PANUTIERRNO_NOTFOUND) {
			job_count = 0;
			break;
		}

		if (rc != (int32_t)PANUTIERRNO_PLAINSUCCESS) {
			break;
		}

		job_remove(exited_pid);
	}
}
