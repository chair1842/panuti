/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "job.h"

#include <panuti/errno.h>
#include <panuti/process.h>
#include <stdlib.h>

static pid_t* job_pids;
static size_t job_cap;
static int job_count;

static bool job_reserve(size_t need) {
	if (need <= job_cap) {
		return true;
	}

	size_t want = job_cap ? job_cap : 16;

	while (want < need) {
		if (want > (size_t)-1 / 2 / sizeof(pid_t)) {
			return false;
		}
		want *= 2;
	}

	pid_t* p = realloc(job_pids, want * sizeof(pid_t));
	if (!p) {
		return false;
	}

	job_pids = p;
	job_cap = want;
	return true;
}

bool pur_job_add(const pid_t* pids, int count) {
	if (!pids || count <= 0) {
		return true;
	}

	if (!job_reserve((size_t)job_count + (size_t)count)) {
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
