/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/process.h>
#include <panuti/syscall/syscallsf.h>

int32_t wait_many(const pid_t* pids, uint32_t count, int* ec_out, pid_t* exit_pid) {
	return panutisysf_wait(pids, count, ec_out, exit_pid);
}
