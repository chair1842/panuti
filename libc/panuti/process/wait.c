/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/process.h>
#include <panuti/syscall/syscallsf.h>

int32_t wait(pid_t pid, int* ec_out) {
	pid_t exited;
	return panutisysf_wait(&pid, 1, ec_out, &exited);
}
